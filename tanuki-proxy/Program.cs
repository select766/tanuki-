using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text.RegularExpressions;
using System.Timers;
using System.Threading;
using System.Threading.Tasks;
using static System.Math;
using static tanuki_proxy.Logging;

namespace tanuki_proxy
{
    public class Program
    {
        public enum UpstreamState
        {
            Stopped,
            Thinking,
        }

        public class EngineBestmove
        {
            public string move { get; set; }
            public string ponder { get; set; }
            public int score { get; set; }
        }

        public static object upstreamLockObject = new object();
        public static UpstreamState upstreamState = UpstreamState.Stopped;
        public static int depth = 0;
        public const string bestmoveNone = "None";
        public static EngineBestmove[] engineBestmoves = null;
        public static string upstreamPosition = "";
        public static int numberOfReadyoks = 0;
        public static System.Random random = new System.Random();
        private static object lastShowPvLockObject = new object();
        private static DateTime lastShowPv = DateTime.Now;
        private static double showPvSupressionMs = 200.0;
        private static DateTime decideMoveLimit = DateTime.MaxValue;
        private const int decideMoveSleepMs = 10;
        private const int moveHorizon = 80;
        private const int maxPly = 127;
        private static volatile bool running = true;

        [DataContract]
        public struct Option
        {
            [DataMember]
            public string name;
            [DataMember]
            public string value;

            public Option(string name, string value)
            {
                this.name = name;
                this.value = value;
            }
        }

        [DataContract]
        public class EngineOption
        {
            [DataMember]
            public string engineName { get; set; }
            [DataMember]
            public string fileName { get; set; }
            [DataMember]
            public string arguments { get; set; }
            [DataMember]
            public string workingDirectory { get; set; }
            [DataMember]
            public Option[] optionOverrides { get; set; }
            [DataMember]
            public bool timeKeeper { get; set; } = false;

            public EngineOption(string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, bool timeKeeper)
            {
                this.engineName = engineName;
                this.fileName = fileName;
                this.arguments = arguments;
                this.workingDirectory = workingDirectory;
                this.optionOverrides = optionOverrides;
                this.timeKeeper = timeKeeper;
            }

            public EngineOption()
            {
                //empty constructor for serialization
            }
        }

        [DataContract]
        public class ProxySetting
        {
            [DataMember]
            public EngineOption[] engines { get; set; }
            [DataMember]
            public string logDirectory { get; set; }
        }

        class Engine
        {
            private readonly Process process = new Process();
            private readonly Option[] optionOverrides;
            private string downstreamPosition = "";
            private readonly object downstreamLockObject = new object();
            private readonly BlockingCollection<string> commandQueue = new BlockingCollection<string>();
            private Thread thread;
            public string name { get; }
            private readonly int id;
            private readonly bool timeKeeper;

            public Engine(string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, int id, bool timeKeeper)
            {
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

            public Engine(EngineOption opt, int id) : this(opt.engineName, opt.fileName, opt.arguments, opt.workingDirectory, opt.optionOverrides, id, opt.timeKeeper)
            {
            }

            public void RunAsync()
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

            public void Close()
            {
                commandQueue.CompleteAdding();
                thread.Join();
                process.Close();
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

            private static string Join(IEnumerable<string> command)
            {
                return string.Join(" ", command);
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

            private int getNumberOfRunningEngines()
            {
                return engines.Count(engine => !engine.process.HasExited);
            }

            private void HandleReadyok(List<string> command)
            {
                // goコマンドが受理されたのでpvの受信を開始する
                lock (upstreamLockObject)
                {
                    if (getNumberOfRunningEngines() == Interlocked.Increment(ref numberOfReadyoks))
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
                lock (upstreamLockObject)
                {
                    // 上流停止中はpvを含む行を処理しない
                    if (upstreamState == UpstreamState.Stopped)
                    {
                        //Log("     ## process={0} upstreamState == UpstreamState.Stopped", process);
                        return;
                    }

                    lock (downstreamLockObject)
                    {
                        // 思考中の局面が違う場合は処理しない
                        if (upstreamPosition != downstreamPosition)
                        {
                            Log("     ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                            return;
                        }
                    }

                    int depthIndex = command.IndexOf("depth");
                    int pvIndex = command.IndexOf("pv");

                    if (depthIndex == -1 || pvIndex == -1)
                    {
                        return;
                    }

                    int tempDepth = int.Parse(command[depthIndex + 1]);
                    int cpIndex = command.IndexOf("cp");

                    Debug.Assert(pvIndex + 1 < command.Count);
                    engineBestmoves[id].move = command[pvIndex + 1];
                    if (pvIndex + 2 < command.Count)
                    {
                        engineBestmoves[id].ponder = command[pvIndex + 2];
                    }
                    if (cpIndex != -1)
                    {
                        int score = int.Parse(command[cpIndex + 1]);
                        engineBestmoves[id].score = score;
                    }

                    Log("<P   [{0}] {1}", id, Join(command));

                    // Fail-low/Fail-highした探索結果は表示しない
                    lock (lastShowPvLockObject)
                    {
                        if (lastShowPv.AddMilliseconds(showPvSupressionMs) < DateTime.Now &&
                            !command.Contains("lowerbound") &&
                            !command.Contains("upperbound") &&
                            depth < tempDepth)
                        {
                            // voteの表示
                            Dictionary<string, int> bestmoveToCount = new Dictionary<string, int>();
                            foreach (var engineBestmove in engineBestmoves)
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
                            WriteLineAndFlush(Console.Out, vote);

                            // info pvの表示
                            WriteLineAndFlush(Console.Out, Join(command));
                            depth = tempDepth;
                            lastShowPv = DateTime.Now;
                        }
                    }
                }
            }

            private void HandleBestmove(List<string> command)
            {
                // 手番かつ他の思考エンジンがbestmoveを返していない時のみ
                // bestmoveを返すようにする
                lock (upstreamLockObject)
                {
                    if (upstreamState == UpstreamState.Stopped)
                    {
                        //Log("     ## process={0} upstreamState == UpstreamState.Stopped", process);
                        return;
                    }

                    lock (downstreamLockObject)
                    {
                        // 思考中の局面が違う場合は処理しない
                        if (upstreamPosition != downstreamPosition)
                        {
                            Log("  ## process={0} upstreamGoIndex != downstreamGoIndex", process);
                            return;
                        }
                    }

                    // TODO(nodchip): この条件が必要かどうか考える
                    if (engineBestmoves == null)
                    {
                        return;
                    }

                    if (command[1] == "resign" || command[1] == "win")
                    {
                        foreach (var engineBestmove in engineBestmoves)
                        {
                            engineBestmove.move = command[1];
                            engineBestmove.ponder = null;
                        }

                        if (command.Count == 4 && command[2] == "ponder")
                        {
                            foreach (var engineBestmove in engineBestmoves)
                            {
                                engineBestmove.move = command[3];
                            }
                        }
                    }

                    DecideMove();
                }
            }
        }

        public class CountAndScore
        {
            public int Count { get; set; } = 0;
            public int Score { get; set; } = int.MinValue;
        }

        /// <summary>
        /// 指し手を決め、bestmoveを出力する
        /// </summary>
        static void DecideMove()
        {
            lock (upstreamLockObject)
            {
                var bestmoveToCount = new Dictionary<string, CountAndScore>();
                foreach (var engineBestmove in engineBestmoves)
                {
                    if (engineBestmove.move == null)
                    {
                        continue;
                    }

                    if (!bestmoveToCount.ContainsKey(engineBestmove.move))
                    {
                        bestmoveToCount.Add(engineBestmove.move, new CountAndScore());
                    }
                    ++bestmoveToCount[engineBestmove.move].Count;
                    bestmoveToCount[engineBestmove.move].Score = Math.Max(bestmoveToCount[engineBestmove.move].Score, engineBestmove.score);
                }

                // 楽観合議制っぽいなにか…。
                // 最も投票の多かった指し手を選ぶ
                // 投票数が同じ場合は最もスコアが高かった指し手を選ぶ
                int bestCount = -1;
                int bestScore = int.MinValue;
                string bestmove = "resign";
                foreach (var p in bestmoveToCount)
                {
                    if (p.Value.Count > bestCount || (p.Value.Count == bestCount && p.Value.Score > bestScore))
                    {
                        bestmove = p.Key;
                        bestCount = p.Value.Count;
                        bestScore = p.Value.Score;
                    }
                }

                // Ponderを決定する
                // 最もスコアが高かった指し手のPonderを選択する
                // スコアが等しいものが複数ある場合はランダムに1つを選ぶ
                int ponderCount = 0;
                string ponder = null;
                foreach (var engineBestmove in engineBestmoves)
                {
                    if (engineBestmove.move != bestmove || engineBestmove.ponder == null || engineBestmove.score != bestScore)
                    {
                        continue;
                    }
                    else if (engineBestmove.ponder == null)
                    {
                        continue;
                    }

                    ++ponderCount;
                    if (random.Next(ponderCount) == 0)
                    {
                        ponder = engineBestmove.ponder;
                    }
                }

                string outputCommand = null;
                if (!string.IsNullOrEmpty(ponder))
                {
                    outputCommand = string.Format("bestmove {0} ponder {1}", bestmove, ponder);
                }
                else
                {
                    outputCommand = string.Format("bestmove {0}", bestmove);
                }

                Log("<P   {0}", outputCommand);
                WriteLineAndFlush(Console.Out, outputCommand);

                TransitUpstreamState(UpstreamState.Stopped);
                depth = 0;
                engineBestmoves = null;
                decideMoveLimit = DateTime.MaxValue;
                WriteToEachEngine("stop");
            }
        }

        static List<Engine> engines = new List<Engine>();

        static int GetProcessId()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return process.Id;
            }
        }

        private static void CheckDecideMoveLimit()
        {
            while (running)
            {
                lock (upstreamLockObject)
                {
                    //Log("     limit={0}", decideMoveLimit.ToString("o"));
                    if (decideMoveLimit < DateTime.Now)
                    {
                        DecideMove();
                    }
                }
                Thread.Sleep(decideMoveSleepMs);
            }
        }

        static void Main(string[] args)
        {
            //writeSampleSetting();
            ProxySetting setting = loadSetting();
            string logFileFormat = Path.Combine(setting.logDirectory, string.Format("tanuki-proxy.{0}.pid={1}.log.txt", DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss"), GetProcessId()));
            Logging.Open(logFileFormat);

            for (int id = 0; id < setting.engines.Length; ++id)
            {
                engines.Add(new Engine(setting.engines[id], id));
            }

            var decideMoveTask = new Thread(CheckDecideMoveLimit);
            decideMoveTask.Start();

            // 子プロセスの標準入出力 (System.Diagnostics.Process) - Programming/.NET Framework/標準入出力 - 総武ソフトウェア推進所 http://smdn.jp/programming/netfx/standard_streams/1_process/
            try
            {
                foreach (var engine in engines)
                {
                    engine.RunAsync();
                }

                int networkDelay = 120;
                int networkDelay2 = 1120;
                int minimumThinkingTime = 2000;
                int maxGamePly = int.MaxValue;
                int time = 0;
                int byoyomi = 0;
                int inc = 0;

                string input;
                while ((input = Console.ReadLine()) != null)
                {
                    Log("U>       {0}", input);
                    var command = Split(input);

                    if (command[0] == "go")
                    {
                        lock (upstreamLockObject)
                        {
                            // 思考開始の合図です。エンジンはこれを受信すると思考を開始します。
                            int btime = ParseCommandParameter(command, "btime");
                            int wtime = ParseCommandParameter(command, "wtime");
                            time = IsBlack() ? btime : wtime;
                            byoyomi = ParseCommandParameter(command, "byoyomi");
                            int binc = ParseCommandParameter(command, "binc");
                            int winc = ParseCommandParameter(command, "winc");
                            inc = IsBlack() ? binc : winc;

                            if (!command.Contains("ponder"))
                            {
                                int maximumTime = CalculateMaximumTime(networkDelay, networkDelay2,
                                    minimumThinkingTime, maxGamePly, time, byoyomi, inc, Ply());
                                decideMoveLimit = DateTime.Now.AddMilliseconds(maximumTime);
                            }

                            engineBestmoves = new EngineBestmove[engines.Count];
                            for (int i = 0; i < engines.Count; ++i)
                            {
                                engineBestmoves[i] = new EngineBestmove();
                            }
                            depth = 0;
                            TransitUpstreamState(UpstreamState.Thinking);
                            WriteLineAndFlush(Console.Out, "info string " + upstreamPosition);
                            lastShowPv = DateTime.Now;
                        }
                    }
                    else if (command[0] == "isready")
                    {
                        lock (upstreamLockObject)
                        {
                            numberOfReadyoks = 0;
                        }
                    }
                    else if (command[0] == "position")
                    {
                        lock (upstreamLockObject)
                        {
                            upstreamPosition = input;
                            Log("     upstreamPosition=" + upstreamPosition);
                        }
                    }
                    else if (command[0] == "ponderhit")
                    {
                        lock (upstreamLockObject)
                        {
                            int maximumTime = CalculateMaximumTime(networkDelay, networkDelay2,
                                minimumThinkingTime, maxGamePly, time, byoyomi, inc, Ply());
                            decideMoveLimit = DateTime.Now.AddMilliseconds(maximumTime);
                        }
                    }
                    else if (command[0] == "stop")
                    {
                        lock (upstreamLockObject)
                        {
                            // stopが来た時に指し手を返さないと次のgoがもらえない
                            DecideMove();
                        }
                    }
                    else if (command[0] == "setoption")
                    {
                        lock (upstreamLockObject)
                        {
                            // setoption name nodestime value 0
                            switch (command[2])
                            {
                                case "NetworkDelay":
                                    networkDelay = int.Parse(command[4]);
                                    break;
                                case "NetworkDelay2":
                                    networkDelay2 = int.Parse(command[4]);
                                    break;
                                case "MinimumThinkingTime":
                                    minimumThinkingTime = int.Parse(command[4]);
                                    break;
                                case "MaxMovesToDraw":
                                    maxGamePly = int.Parse(command[4]);
                                    if (maxGamePly == 0)
                                    {
                                        maxGamePly = int.MaxValue;
                                    }
                                    break;
                            }
                        }
                    }

                    WriteToEachEngine(input);

                    if (input == "quit")
                    {
                        break;
                    }
                }
            }
            finally
            {
                running = false;
                decideMoveTask.Join();
                Logging.Close();
                foreach (var engine in engines)
                {
                    engine.Close();
                }
            }
        }

        private static int ParseCommandParameter(List<string> command, string parameter)
        {
            if (!command.Contains(parameter))
            {
                return 0;
            }
            int index = command.IndexOf(parameter);
            return int.Parse(command[index + 1]);
        }

        private static int CalculateMaximumTime(int networkDelay, int networkDelay2,
            int minimumThinkingTime, int maxGamePly, int time, int byoyomi, int inc, int ply)
        {
            //WriteLineAndFlush(Console.Out, "into string CalculateMaximumTime()");
            maxGamePly = Min(maxGamePly, ply + maxPly - 1);

            // 今回の最大残り時間(これを超えてはならない)
            int remain_time = time + byoyomi - networkDelay2;
            // ここを0にすると時間切れのあと自爆するのでとりあえず100にしておく。
            remain_time = Max(remain_time, 100);

            // 残り手数
            // plyは開始局面が1。
            // なので、256手ルールとして、max_game_ply == 256
            // 256手目の局面においてply == 256
            // その1手前の局面においてply == 255
            // これらのときにMTGが1にならないといけない。
            // だから2足しておくのが正解。
            int MTG = Min((maxGamePly - ply + 2) / 2, moveHorizon);

            if (MTG <= 0)
            {
                // 本来、終局までの最大手数が指定されているわけだから、この条件で呼び出されるはずはないのだが…。
                WriteLineAndFlush(Console.Out, "info string max_game_ply is too small.");
                return inc - networkDelay2;
            }
            if (MTG == 1)
            {
                // この手番で終了なので使いきれば良い。
                return remain_time;
            }

            // minimumとoptimumな時間を適当に計算する。

            // 最小思考時間(これが1000より短く設定されることはないはず..)
            int minimumTime = Max(minimumThinkingTime - networkDelay, 1000);

            // 最適思考時間と、最大思考時間には、まずは上限値を設定しておく。
            int maximumTime = remain_time;

            // maximumTime = min ( minimumTime + α * 5 , remain_time)
            // みたいな感じで考える

            // 残り手数において残り時間はあとどれくらいあるのか。
            int remain_estimate = time
                + inc * MTG
                // 秒読み時間も残り手数に付随しているものとみなす。
                + byoyomi * MTG;

            // 1秒ずつは絶対消費していくねんで！
            remain_estimate -= (MTG + 1) * 1000;
            remain_estimate = Max(remain_estimate, 0);

            // -- maximumTime
            double max_ratio = 5.0f;
            // 切れ負けルールにおいては、5分を切っていたら、このratioを抑制する。
            if (inc == 0 && byoyomi == 0)
            {
                // 3分     : ratio = 3.0
                // 2分     : ratio = 2.0
                // 1分以下 : ratio = 1.0固定
                max_ratio = Min(max_ratio, Max((double)time / (60 * 1000), 1.0f));
            }
            int t2 = minimumTime + (int)(remain_estimate * max_ratio / MTG);

            // ただしmaximumは残り時間の30%以上は使わないものとする。
            // optimumが超える分にはいい。それは残り手数が少ないときとかなので構わない。
            t2 = Min(t2, (int)(remain_estimate * 0.3));

            maximumTime = Min(t2, maximumTime);

            // 秒読みモードでかつ、持ち時間がないなら、最小思考時間も最大思考時間もその時間にしたほうが得
            if (byoyomi > 0)
            {
                // 持ち時間が少ないなら(秒読み時間の1.2倍未満なら)、思考時間を使いきったほうが得
                // これには持ち時間がゼロのケースも含まれる。
                if (time < (int)(byoyomi * 1.2))
                {
                    maximumTime = byoyomi + time;
                }
            }

            // 残り時間 - network_delay2よりは短くしないと切れ負けになる可能性が出てくる。
            maximumTime = Min(roundUp(maximumTime, minimumThinkingTime, networkDelay, remain_time), remain_time);
            WriteLineAndFlush(Console.Out, "into string maximumTime {0}", maximumTime);
            Log("     maximumTime {0}", maximumTime);
            return maximumTime;
        }

        private static int roundUp(int t, int minimumThinkingTime, int networkDelay, int remainTime)
        {
            // 1000で繰り上げる。Options["MinimalThinkingTime"]が最低値。
            t = Max(((t + 999) / 1000) * 1000, minimumThinkingTime);
            // そこから、Options["NetworkDelay"]の値を引くが、remain_timeを上回ってはならない。
            t = Min(t - networkDelay, remainTime);
            return t;
        }

        /// <summary>
        /// エンジンに対して出力する
        /// </summary>
        /// <param name="input">親ソフトウェアからの入力。USIプロトコルサーバーまたは親tanuki-proxy</param>
        private static void WriteToEachEngine(string input)
        {
            foreach (var engine in engines)
            {
                engine.Write(input);
                if (input == "usi")
                {
                    break;
                }
            }
        }

        private static bool IsBlack()
        {
            lock (upstreamLockObject)
            {
                return Ply() % 2 == 1;
            }
        }

        /// <summary>
        /// (将棋の)開始局面からの手数を返す。平手の開始局面なら1が返る。(0ではない)
        /// </summary>
        /// <returns>開始局面からの手数</returns>
        private static int Ply()
        {
            lock (upstreamLockObject)
            {
                var position = Split(upstreamPosition);
                int index = position.IndexOf("moves");
                if (index == -1)
                {
                    return 1;
                }
                return position.Count - index - 1;
            }
        }

        static List<string> Split(string s)
        {
            return new List<string>(new Regex("\\s+").Split(s));
        }

        static void TransitUpstreamState(UpstreamState newUpstreamState)
        {
            Log("     {0} > {1}", upstreamState, newUpstreamState);
            upstreamState = newUpstreamState;
        }

        static void writeSampleSetting()
        {
            ProxySetting setting = new ProxySetting();
            setting.logDirectory = "C:\\home\\develop\\tanuki-";
            setting.engines = new EngineOption[]
            {
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                },true),
            new EngineOption(
                "nighthawk",
                "ssh",
                "-vvv nighthawk tanuki-.bat",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "4"),
                },false),
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                },false)
            };

            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(ProxySetting));
            using (FileStream f = new FileStream("proxy-setting.sample.json", FileMode.Create))
            {
                serializer.WriteObject(f, setting);
            }
        }

        static string ExeDir()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return Path.GetDirectoryName(process.MainModule.FileName);
            }
        }

        static ProxySetting loadSetting()
        {
            DataContractJsonSerializer serializer = new DataContractJsonSerializer(typeof(ProxySetting));
            // 1. まずはexeディレクトリに設定ファイルがあれば使う(複数Proxy設定をexeごとディレクトリに分け、カレントディレクトリは制御できない場合)
            // 2. それが無ければ、カレントディレクトリの設定を使う
            string[] search_dirs = { ExeDir(), "." };
            foreach (string search_dir in search_dirs)
            {
                string path = Path.Combine(search_dir, "proxy-setting.json");
                if (File.Exists(path))
                {
                    using (FileStream f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        return (ProxySetting)serializer.ReadObject(f);
                    }
                }
            }
            return null;
        }
        private static void WriteLineAndFlush(TextWriter textWriter, string format, params object[] arg)
        {
            textWriter.WriteLine(format, arg);
            textWriter.Flush();
        }
    }
}
