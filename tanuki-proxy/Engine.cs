using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using static tanuki_proxy.Logging;
using static tanuki_proxy.Program;
using static tanuki_proxy.Setting;
using static tanuki_proxy.Utility;

namespace tanuki_proxy
{
    public class Engine : IDisposable
    {
        private const double showPvSupressionMs = 200.0;
        private const int mateScore = 32000;

        bool disposed = false;
        private readonly Program program;
        private readonly Process process = new Process();
        private readonly Option[] optionOverrides;
        private string downstreamPosition = "";
        private readonly object downstreamLockObject = new object();
        private readonly BlockingCollection<string> commandQueue = new BlockingCollection<string>();
        private Thread thread;
        public string name { get; }
        private readonly int id;
        private readonly bool timeKeeper;
        public bool ProcessHasExited { get { return process.HasExited; } }

        public Engine(Program program, string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, int id, bool timeKeeper)
        {
            this.program = program;
            this.name = engineName;
            this.process.StartInfo.FileName = fileName;
            this.process.StartInfo.Arguments = arguments;
            this.process.StartInfo.WorkingDirectory = workingDirectory;
            this.process.StartInfo.UseShellExecute = false;
            this.process.StartInfo.RedirectStandardInput = true;
            this.process.StartInfo.RedirectStandardOutput = true;
            this.process.StartInfo.RedirectStandardError = true;
            this.process.OutputDataReceived += HandleStdout;
            this.process.ErrorDataReceived += HandleStderr;
            this.optionOverrides = optionOverrides;
            this.id = id;
            this.timeKeeper = timeKeeper;
        }

        public Engine(Program program, EngineOption opt, int id) : this(program, opt.engineName, opt.fileName, opt.arguments, opt.workingDirectory, opt.optionOverrides, id, opt.timeKeeper)
        {
        }

        public void Start()
        {
            thread = new Thread(ThreadRun);
            thread.Start();
        }

        private void ThreadRun()
        {
            process.Start();
            process.BeginOutputReadLine();
            process.BeginErrorReadLine();

            while (!commandQueue.IsCompleted)
            {
                string input = null;
                try
                {
                    input = commandQueue.Take();
                }
                catch (InvalidOperationException)
                {
                    continue;
                }

                if (string.IsNullOrEmpty(input))
                {
                    continue;
                }

                var command = Split(input);
                if (command.Count == 0)
                {
                    continue;
                }

                // 将棋所：USIプロトコルとは http://www.geocities.jp/shogidokoro/usi.html
                if (command.Contains("setoption"))
                {
                    HandleSetoption(command);
                }
                else if (command.Contains("go"))
                {
                    HandleGo(command);
                }
                else if (command.Contains("ponderhit"))
                {
                    HandlePonderhit(command);
                }
                else
                {
                    Log("  P> [{0}] {1}", id, Join(command));
                    WriteLineAndFlush(process.StandardInput, Join(command));
                }
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposed)
            {
                return;
            }

            commandQueue.CompleteAdding();
            thread.Join();
            commandQueue.Dispose();
            process.Dispose();

            disposed = true;
        }

        /// <summary>
        /// エンジンにコマンドを書き込む。
        /// </summary>
        /// <param name="input"></param>
        public void Write(string input)
        {
            // コマンドをBlockingCollectionに入れる。
            // 入れられたコマンドは別のスレッドで処理される。
            commandQueue.Add(input);
        }

        /// <summary>
        /// setoptionコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandleSetoption(List<string> command)
        {
            // エンジンに対して値を設定する時に送ります。
            Debug.Assert(command.Count == 5);
            Debug.Assert(command[1] == "name");
            Debug.Assert(command[3] == "value");

            // オプションをオーバーライドする
            foreach (var optionOverride in optionOverrides)
            {
                if (command[2] == optionOverride.name)
                {
                    command[4] = optionOverride.value;
                }
            }

            Log("  P> [{0}] {1}", id, Join(command));
            WriteLineAndFlush(process.StandardInput, Join(command));
        }

        /// <summary>
        /// goコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandleGo(List<string> command)
        {
            if (timeKeeper)
            {
                Log("  P> [{0}] {1}", id, Join(command));
                WriteLineAndFlush(process.StandardInput, Join(command));
            }
            else
            {
                string output = "go infinite";
                Log("  P> [{0}] {1}", id, output);
                WriteLineAndFlush(process.StandardInput, output);
            }
        }

        /// <summary>
        /// ponderhitコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandlePonderhit(List<string> command)
        {
            if (timeKeeper)
            {
                Log("  P> [{0}] {1}", id, Join(command));
                WriteLineAndFlush(process.StandardInput, Join(command));
            }
            // タイムキーパー以外にはそのまま思考を続けさせる
        }

        /// <summary>
        /// 思考エンジンの標準出力を処理する
        /// </summary>
        /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
        /// <param name="e">思考エンジンの出力</param>
        private void HandleStdout(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e.Data))
            {
                return;
            }

            Log("  <D [{0}] {1}", id, e.Data);

            var command = Split(e.Data);

            if (command.Contains("readyok"))
            {
                HandleReadyok(command);
            }
            else if (command.Contains("position"))
            {
                HandlePosition(command);
            }
            else if (command.Contains("pv"))
            {
                HandlePv(command);
            }
            else if (command.Contains("bestmove"))
            {
                HandleBestmove(command);
            }
            else
            {
                Log("<P   [{0}] {1}", id, Join(command));
                WriteLineAndFlush(Console.Out, Join(command));
            }
        }

        /// <summary>
        /// 思考エンジンの標準エラー出力を処理する
        /// </summary>
        /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
        /// <param name="e">思考エンジンの出力</param>
        private void HandleStderr(object sender, DataReceivedEventArgs e)
        {
            if (string.IsNullOrEmpty(e.Data))
            {
                return;
            }
            Log("  <D [{0}] {1}", id, e.Data);
        }

        private void HandleReadyok(List<string> command)
        {
            // goコマンドが受理されたのでpvの受信を開始する
            lock (program.UpstreamLockObject)
            {
                if (program.NumberOfRunningEngines == Interlocked.Increment(ref program.numberOfReadyoks))
                {
                    Log("<P   [{0}] {1}", id, Join(command));
                    WriteLineAndFlush(Console.Out, Join(command));
                }
            }
        }

        /// <summary>
        /// info string startedを受信し、goコマンドが受理されたときの処理を行う
        /// </summary>
        /// <param name="command">思考エンジンの出力文字列</param>
        private void HandlePosition(List<string> command)
        {
            // goコマンドが受理されたのでpvの受信を開始する
            lock (downstreamLockObject)
            {
                int index = command.IndexOf("position");
                downstreamPosition = Join(command.Skip(index));
                Log("     downstreamPosition=" + downstreamPosition);
            }
        }

        /// <summary>
        /// pvを含むinfoコマンドを処理する
        /// </summary>
        /// <param name="command">思考エンジンの出力文字列</param>
        private void HandlePv(List<string> command)
        {
            lock (program.UpstreamLockObject)
            {
                // 上流停止中はpvを含む行を処理しない
                if (program.UpstreamState == UpstreamStateEnum.Stopped)
                {
                    //Log("     ## process={0} upstreamState == UpstreamStateEnum.Stopped", process);
                    return;
                }

                lock (downstreamLockObject)
                {
                    // 思考中の局面が違う場合は処理しない
                    if (program.UpstreamPosition != downstreamPosition)
                    {
                        Log("     ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                        return;
                    }
                }

                // 定跡の列挙の場合は処理しない
                if (Join(command).Contains("%)"))
                {
                    return;
                }

                int depthIndex = command.IndexOf("depth");
                int pvIndex = command.IndexOf("pv");
                int mateIndex = command.IndexOf("mate");

                if (depthIndex == -1 || pvIndex == -1)
                {
                    return;
                }

                program.EngineBestmoves[id].command = command;

                int tempDepth = int.Parse(command[depthIndex + 1]);

                // 詰まされた場合
                if (tempDepth == 0)
                {
                    program.EngineBestmoves[id].move = "resign";
                    program.EngineBestmoves[id].ponder = null;
                    return;
                }

                Debug.Assert(pvIndex + 1 < command.Count);
                program.EngineBestmoves[id].move = command[pvIndex + 1];
                if (pvIndex + 2 < command.Count)
                {
                    program.EngineBestmoves[id].ponder = command[pvIndex + 2];
                }

                int cpIndex = command.IndexOf("cp");
                if (cpIndex != -1)
                {
                    int score = int.Parse(command[cpIndex + 1]);
                    program.EngineBestmoves[id].score = score;
                }

                if (mateIndex != -1)
                {
                    int mate = int.Parse(command[mateIndex + 1]);
                    if (mate > 0)
                    {
                        program.EngineBestmoves[id].score = mateScore - mate;
                    }
                    else
                    {
                        program.EngineBestmoves[id].score = -mateScore - mate;
                    }
                }

                int npsIndex = command.IndexOf("nps");
                if (npsIndex != -1)
                {
                    int nps = int.Parse(command[npsIndex + 1]);
                    program.EngineBestmoves[id].nps = nps;
                }

                // Fail-low/Fail-highした探索結果は表示しない
                lock (program.LastShowPvLockObject)
                {
                    // 深さ3未満のPVを出力するのは将棋所にmateの値を認識させるため
                    if ((tempDepth < 3 || program.LastShowPv.AddMilliseconds(showPvSupressionMs) < DateTime.Now) &&
                        !command.Contains("lowerbound") &&
                        !command.Contains("upperbound") &&
                        program.Depth < tempDepth)
                    {
                        // voteの表示
                        Dictionary<string, int> bestmoveToCount = new Dictionary<string, int>();
                        foreach (var engineBestmove in program.EngineBestmoves)
                        {
                            if (engineBestmove.move == null)
                            {
                                continue;
                            }

                            if (!bestmoveToCount.ContainsKey(engineBestmove.move))
                            {
                                bestmoveToCount.Add(engineBestmove.move, 0);
                            }
                            ++bestmoveToCount[engineBestmove.move];
                        }
                        List<KeyValuePair<string, int>> bestmoveAndCount = new List<KeyValuePair<string, int>>(bestmoveToCount);
                        bestmoveAndCount.Sort((KeyValuePair<string, int> lh, KeyValuePair<string, int> rh) => { return -(lh.Value - rh.Value); });

                        string vote = "info string";
                        foreach (var p in bestmoveAndCount)
                        {
                            vote += " ";
                            vote += p.Key;
                            vote += "=";
                            vote += p.Value;
                        }
                        Log("<P   {0}", vote);
                        WriteLineAndFlush(Console.Out, vote);

                        // info pvの表示
                        int sumNps = program.EngineBestmoves.Sum(x => x.nps);
                        var commandWithNps = new List<string>(command);
                        int sumNpsIndex = commandWithNps.IndexOf("nps");
                        if (sumNpsIndex != -1)
                        {
                            commandWithNps[sumNpsIndex + 1] = sumNps.ToString();
                        }
                        Log("<P   {0}", Join(commandWithNps));
                        WriteLineAndFlush(Console.Out, Join(commandWithNps));
                        program.Depth = tempDepth;
                        program.LastShowPv = DateTime.Now;
                    }
                }
            }
        }

        private void HandleBestmove(List<string> command)
        {
            // 手番かつ他の思考エンジンがbestmoveを返していない時のみ
            // bestmoveを返すようにする
            lock (program.UpstreamLockObject)
            {
                if (program.UpstreamState == UpstreamStateEnum.Stopped)
                {
                    //Log("     ## process={0} upstreamState == UpstreamStateEnum.Stopped", process);
                    return;
                }

                lock (downstreamLockObject)
                {
                    // 思考中の局面が違う場合は処理しない
                    if (program.UpstreamPosition != downstreamPosition)
                    {
                        Log("  ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                        return;
                    }
                }

                // TODO(nodchip): この条件が必要かどうか考える
                if (program.EngineBestmoves == null)
                {
                    return;
                }

                if (command[1] == "resign" || command[1] == "win")
                {
                    foreach (var engineBestmove in program.EngineBestmoves)
                    {
                        engineBestmove.move = command[1];
                        engineBestmove.ponder = null;
                    }

                    if (command.Count == 4 && command[2] == "ponder")
                    {
                        foreach (var engineBestmove in program.EngineBestmoves)
                        {
                            engineBestmove.move = command[3];
                        }
                    }
                }

                program.DecideMove();
            }
        }
    }
}
