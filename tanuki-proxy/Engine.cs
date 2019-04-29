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
        public class EngineBestmove
        {
            public string move { get; set; }
            public string ponder { get; set; }
            public int score { get; set; }
            public List<string> pvCommand { get; set; }
            public long nps { get; set; }
        }

        private const double showPvSupressionMs = 200.0;
        private const int mateScore = 32000;

        bool disposed = false;
        private readonly Program program;
        private readonly Process process = new Process();
        private readonly Option[] optionOverrides;
        public string ExpectedDownstreamPosition { get; set; } = null;
        public string ExpectedDownstreamGo { get; set; } = null;
        private string actualDownstreamPosition = "";
        private string actualDownstreamGo = "";
        private readonly object downstreamLockObject = new object();
        private readonly BlockingCollection<string> commandQueue = new BlockingCollection<string>();
        private Thread thread;
        public string name { get; }
        private readonly int id;
        public bool TimeKeeper { get; set; }
        public bool ProcessHasExited { get { return process.HasExited; } }
        public EngineBestmove Bestmove { get; set; }
        public bool MateEngine { get; set; }
        private readonly ManualResetEvent eventMultipv = new ManualResetEvent(true);
        private string[] multipvMoves;
        private readonly ManualResetEvent eventSearching = new ManualResetEvent(true);

        public Engine(Program program, string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, int id, bool mateEngine)
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
            this.TimeKeeper = false;
            this.MateEngine = mateEngine;
        }

        public Engine(Program program, EngineOption opt, int id) : this(program, opt.engineName, opt.fileName, opt.arguments, opt.workingDirectory, opt.optionOverrides, id, opt.mateEngine)
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
                    HandleUpstreamSetoption(command);
                }
                else if (command.Contains("position"))
                {
                    HandleUpstreamPosition(command);
                }
                else if (command.Contains("go"))
                {
                    HandleUpstreamGo(command);
                }
                else if (command.Contains("ponderhit"))
                {
                    HandleUpstreamPonderhit(command);
                }
                else if (command.Contains("tt"))
                {
                    // ttコマンドは量が多いためログに出力しないようにする
                    WriteLineAndFlush(process.StandardInput, Join(command));
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
        private void HandleUpstreamSetoption(List<string> command)
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
        /// positionコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandleUpstreamPosition(List<string> command)
        {
            var input = Join(command);
            ExpectedDownstreamPosition = input;
            actualDownstreamPosition = null;
            Log("  P> [{0}] {1}", id, input);
            WriteLineAndFlush(process.StandardInput, input);
        }

        /// <summary>
        /// goコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandleUpstreamGo(List<string> command)
        {
            var input = Join(command);
            ExpectedDownstreamGo = input;
            actualDownstreamGo = null;
            // eventSearchingを非シグナル状態にし、スレッドをブロックする
            eventSearching.Reset();
            Log("  P> [{0}] {1}", id, input);
            WriteLineAndFlush(process.StandardInput, input);
        }

        /// <summary>
        /// ponderhitコマンドを処理する
        /// </summary>
        /// <param name="command">UIまたは上流proxyからのコマンド文字列</param>
        private void HandleUpstreamPonderhit(List<string> command)
        {
            if (TimeKeeper)
            {
                Log("  P> [{0}] {1}", id, Join(command));
                WriteLineAndFlush(process.StandardInput, Join(command));
            }
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

            var command = Split(e.Data);

            if (!command.Contains("tt"))
            {
                // ttコマンドは量が多いのでログに出力しないようにする
                Log("  <D [{0}] {1}", id, e.Data);
            }

            if (command.Contains("tt"))
            {
                program.WriteToOtherEngines(e.Data, this);
            }
            else if (command.Contains("readyok"))
            {
                HandleDownstreamReadyok(command);
            }
            else if (command.Contains("position"))
            {
                HandleDownstreamPosition(command);
            }
            else if (command.Contains("go"))
            {
                HandleDownstreamGo(command);
            }
            else if (command.Contains("pv"))
            {
                HandleDownstreamPv(command);
            }
            else if (command.Contains("bestmove"))
            {
                HandleDownstreamBestmove(command);
            }
            else if (command.Contains("checkmate"))
            {
                HandleDownstreamCheckmate(command);
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

        private void HandleDownstreamReadyok(List<string> command)
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
        /// info string positionを受信し、positionコマンドが受理されたときの処理を行う
        /// </summary>
        /// <param name="command">思考エンジンの出力文字列</param>
        private void HandleDownstreamPosition(List<string> command)
        {
            // positionコマンドが受理された
            // positionコマンドとgoコマンドの両方が受信されたらpv等の受信を始める
            lock (downstreamLockObject)
            {
                int index = command.IndexOf("position");
                actualDownstreamPosition = Join(command.Skip(index));
                Log("     [{0}] . actualDownstreamPosition={1}", id, actualDownstreamPosition);
            }
        }

        /// <summary>
        /// info string goを受信し、goコマンドが受理されたときの処理を行う
        /// </summary>
        /// <param name="command">思考エンジンの出力文字列</param>
        private void HandleDownstreamGo(List<string> command)
        {
            // goコマンドが受理された
            // positionコマンドとgoコマンドの両方が受信されたらpv等の受信を始める
            lock (downstreamLockObject)
            {
                int index = command.IndexOf("go");
                actualDownstreamGo = Join(command.Skip(index));
                Log("     [{0}] . actualDownstreamGo={1}", id, actualDownstreamGo);
            }
        }

        /// <summary>
        /// pvを含むinfoコマンドを処理する
        /// </summary>
        /// <param name="command">思考エンジンの出力文字列</param>
        private void HandleDownstreamPv(List<string> command)
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
                    if (ExpectedDownstreamPosition != actualDownstreamPosition || ExpectedDownstreamGo != actualDownstreamGo)
                    {
                        Log("     [{0}] # process={1} ExpectedDownstreamPosition(={2}) != actualDownstreamPosition(={3})",
                            id, process, ExpectedDownstreamPosition, actualDownstreamPosition);
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
                int multipvIndex = command.IndexOf("multipv");

                if (depthIndex == -1 || pvIndex == -1)
                {
                    return;
                }

                // Multi PV探索の結果を格納する
                if (multipvIndex != -1)
                {
                    //if (multipvMoves == null)
                    //{
                    //    Debugger.Launch();
                    //}
                    Debug.Assert(multipvMoves != null);
                    if (pvIndex + 1 == command.Count)
                    {
                        // ここは通らないはず
                        return;
                    }

                    string move = command[pvIndex + 1];
                    int multipv = int.Parse(command[multipvIndex + 1]);
                    multipvMoves[multipv - 1] = move;
                    return;
                }
                else if (multipvMoves != null && multipvMoves.Length == 1)
                {
                    // ノードが1個しかない場合の処理
                    if (pvIndex + 1 < command.Count)
                    {
                        string move = command[pvIndex + 1];
                        multipvMoves[0] = move;
                        return;
                    }
                }

                // Root局面のpv出力時に合算した値を出力できるように
                // Root局面以外でもnpsの値を保持しておく
                int npsIndex = command.IndexOf("nps");
                if (npsIndex != -1)
                {
                    long nps = long.Parse(command[npsIndex + 1]);
                    Bestmove.nps = nps;
                }

                // Root局面の子局面を探索していた場合は投票しない
                if (program.UpstreamPosition != ExpectedDownstreamPosition)
                {
                    return;
                }

                Bestmove.pvCommand = command;

                int tempDepth = int.Parse(command[depthIndex + 1]);

                // 詰まされた場合
                if (tempDepth == 0)
                {
                    Bestmove.move = "resign";
                    Bestmove.ponder = null;
                    return;
                }

                Debug.Assert(pvIndex + 1 < command.Count);
                Bestmove.move = command[pvIndex + 1];
                if (pvIndex + 2 < command.Count && command[pvIndex + 2] != "score")
                {
                    Bestmove.ponder = command[pvIndex + 2];
                }

                int cpIndex = command.IndexOf("cp");
                if (cpIndex != -1)
                {
                    int score = int.Parse(command[cpIndex + 1]);
                    Bestmove.score = score;
                }

                if (mateIndex != -1)
                {
                    if (command[mateIndex + 1] == "+")
                    {
                        Bestmove.score = 31500;
                    }
                    else if (command[mateIndex + 1] == "-")
                    {
                        Bestmove.score = -31500;
                    }
                    else
                    {
                        int mate = int.Parse(command[mateIndex + 1]);
                        if (mate > 0)
                        {
                            Bestmove.score = mateScore - mate;
                        }
                        else
                        {
                            Bestmove.score = -mateScore - mate;
                        }
                    }
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
                        //// voteの表示
                        //Dictionary<string, int> bestmoveToCount = new Dictionary<string, int>();
                        //foreach (var engine in program.engines)
                        //{
                        //    if (engine.Bestmove.move == null)
                        //    {
                        //        continue;
                        //    }

                        //    if (!bestmoveToCount.ContainsKey(engine.Bestmove.move))
                        //    {
                        //        bestmoveToCount.Add(engine.Bestmove.move, 0);
                        //    }
                        //    ++bestmoveToCount[engine.Bestmove.move];
                        //}
                        //List<KeyValuePair<string, int>> bestmoveAndCount = new List<KeyValuePair<string, int>>(bestmoveToCount);
                        //bestmoveAndCount.Sort((KeyValuePair<string, int> lh, KeyValuePair<string, int> rh) => { return -(lh.Value - rh.Value); });

                        //string vote = "info string";
                        //foreach (var p in bestmoveAndCount)
                        //{
                        //    vote += " ";
                        //    vote += p.Key;
                        //    vote += "=";
                        //    vote += p.Value;
                        //}
                        //Log("<P   {0}", vote);
                        //WriteLineAndFlush(Console.Out, vote);

                        // info pvの表示
                        long sumNps = program.engines
                            .Select(x => x.Bestmove)
                            .Sum(x => x.nps);
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

        private void HandleDownstreamBestmove(List<string> command)
        {
            // Multi Ponder の探索局面を探索中に bestmove を返した場合に
            // eventBestmove のスレッドをを進行する
            // 以下の if 文の条件がないと、ひとつ前の探索で bestmove を受け取ったときに
            // eventBestmove のスレッドを進行してしまう
            if (ExpectedDownstreamPosition == actualDownstreamPosition && ExpectedDownstreamGo == actualDownstreamGo)
            {
                eventMultipv.Set();
            }

            // eventSearchingをシグナル状態にし、スレッドを進行可能な状態にする
            // eventMultipvより先にシグナル状態にし、一つ前の手でスレッドが進行しないようにする
            eventSearching.Set();

            // タイムキーパー以外はbestmoveを無視する
            if (!TimeKeeper)
            {
                return;
            }

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
                    if (program.UpstreamPosition != ExpectedDownstreamPosition || ExpectedDownstreamPosition != actualDownstreamPosition || ExpectedDownstreamGo != actualDownstreamGo)
                    {
                        Log("  ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                        return;
                    }
                }

                if (command[1] == "resign" || command[1] == "win")
                {
                    foreach (var engine in program.engines)
                    {
                        engine.Bestmove.move = command[1];
                        engine.Bestmove.ponder = null;
                    }

                    if (command.Count == 4 && command[2] == "ponder")
                    {
                        foreach (var engine in program.engines)
                        {
                            engine.Bestmove.ponder = command[3];
                        }
                    }
                }
                else if (Bestmove.move == null)
                {
                    // Apery形式の定跡データベースを使用する場合はここに入る
                    Bestmove.move = command[1];
                    if (command.Count == 4 && command[2] == "ponder")
                    {
                        Bestmove.ponder = command[3];
                    }

                }

                program.DecideMove();
            }
        }

        private void HandleDownstreamCheckmate(List<string> command)
        {
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
                    if (program.UpstreamPosition != ExpectedDownstreamPosition || ExpectedDownstreamPosition != actualDownstreamPosition || ExpectedDownstreamGo != actualDownstreamGo)
                    {
                        Log("  ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                        return;
                    }
                }

                // nomate は処理しない
                if (command[1] == "nomate")
                {
                    return;
                }

                Bestmove.move = command[1];
                if (command.Count > 2)
                {
                    Bestmove.ponder = command[2];
                }
                Bestmove.score = mateScore - command.Count + 1;

                string infoStringCommand = "info string " + Join(command);
                Log("<P   [{0}] {1}", id, infoStringCommand);
                WriteLineAndFlush(Console.Out, infoStringCommand);
            }
        }

        public string[] SearchWithMultiPv(string positionCommand, int multiPv)
        {
            Write(positionCommand);

            // 現在の探索が終了するまで待機する
            eventSearching.WaitOne();

            Write(string.Format("setoption name MultiPV value {0}", multiPv));
            multipvMoves = new string[multiPv];

            // multipv探索結果がかえってくるまで待機できるよう
            // eventMultipvを非シグナル状態にし、スレッドをブロックする
            // eventSearchingとeventMultipvは統一しない
            // 統一した場合、goコマンドがエンジンに出力される前に下のWaitOne()に到達し、
            // multipv探索の結果を受け取らないまま進行してい舞う場合が考えられる
            eventMultipv.Reset();

            Write("go byoyomi 100");

            // multipv探索の結果が帰ってくるまで待機する
            eventMultipv.WaitOne();

            Write("setoption name MultiPV value 1");

            var result = multipvMoves;
            multipvMoves = null;
            return result;
        }
    }
}
