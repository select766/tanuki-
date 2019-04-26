using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using static System.Math;
using static System.String;
using static tanuki_proxy.Logging;
using static tanuki_proxy.Setting;
using static tanuki_proxy.Utility;

namespace tanuki_proxy
{
    public class Program : IDisposable
    {
        public enum UpstreamStateEnum
        {
            Stopped,
            Thinking,
        }

        public class CountAndScore
        {
            public int Count { get; set; } = 0;
            public int Score { get; set; } = int.MinValue;
        }

        private const int decideMoveSleepMs = 10;
        private const int moveHorizon = 80;
        private const int maxPly = 127;
        private const string OptionNameMultiPV = "MultiPV";

        private bool disposed = false;
        public object UpstreamLockObject { get; } = new object();
        private UpstreamStateEnum upstreamState = UpstreamStateEnum.Stopped;
        public UpstreamStateEnum UpstreamState
        {
            get { return upstreamState; }
            set
            {
                Log("     {0} > {1}", upstreamState, value);
                upstreamState = value;
            }
        }
        public int Depth { get; set; } = 0;
        public string UpstreamPosition { get; set; }
        public int numberOfReadyoks = 0;
        private readonly System.Random random = new System.Random();
        public object LastShowPvLockObject { get; set; } = new object();
        public DateTime LastShowPv { get; set; } = DateTime.Now;
        private DateTime decideMoveLimit = DateTime.MaxValue;
        private volatile bool running = true;
        public readonly List<Engine> engines = new List<Engine>();
        private Thread decideMoveTask;
        private List<String> lastGoCommand = null;

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

            running = false;
            decideMoveTask.Join();
            Logging.Close();
            foreach (var engine in engines)
            {
                engine.Dispose();
            }

            disposed = true;
        }

        public int NumberOfRunningEngines
        {
            get
            {
                return engines.Count(engine => !engine.ProcessHasExited);
            }
        }

        /// <summary>
        /// 指し手を決め、bestmoveを出力する
        /// </summary>
        public void DecideMove()
        {
            lock (UpstreamLockObject)
            {
                if (engines.Select(x => x.Bestmove).All(x => x.move == null))
                {
                    // いずれのスレーブもpvを返していない状態で
                    // 上流からstopが渡ってきたか、maximumTime経過した
                    Log("<P   {0}", "bestmove resign");
                    WriteLineAndFlush(Console.Out, "bestmove resign");
                }
                else if (engines.Select(x => x.Bestmove).Any(x => x.move == "resign"))
                {
                    // 投了
                    Log("<P   {0}", "bestmove resign");
                    WriteLineAndFlush(Console.Out, "bestmove resign");

                }
                else if (engines.Select(x => x.Bestmove).Any(x => x.move == "win"))
                {
                    //宣言勝ち
                    Log("<P   {0}", "bestmove win");
                    WriteLineAndFlush(Console.Out, "bestmove win");
                }
                else
                {
                    // 詰み・優等局面となる指し手は最優先で指す
                    var maxScoreBestMove = engines[0].Bestmove;
                    foreach (var engine in engines)
                    {
                        if (maxScoreBestMove.score < engine.Bestmove.score)
                        {
                            maxScoreBestMove = engine.Bestmove;
                        }
                    }

                    Engine.EngineBestmove bestmove = null;
                    if (maxScoreBestMove.score > 30000)
                    {
                        bestmove = maxScoreBestMove;
                    }
                    else
                    {
                        // 楽観合議制っぽいなにか…。
                        // 各指し手の投票数を数える
                        var bestmoveToCount = new Dictionary<string, CountAndScore>();
                        foreach (var engine in engines)
                        {
                            if (engine.Bestmove.move == null)
                            {
                                continue;
                            }

                            if (!bestmoveToCount.ContainsKey(engine.Bestmove.move))
                            {
                                bestmoveToCount.Add(engine.Bestmove.move, new CountAndScore());
                            }
                            ++bestmoveToCount[engine.Bestmove.move].Count;
                            bestmoveToCount[engine.Bestmove.move].Score = Math.Max(bestmoveToCount[engine.Bestmove.move].Score, engine.Bestmove.score);
                        }

                        // 最も得票数の高い指し手を選ぶ
                        // 得票数が同じ場合は最も評価値の高い手を選ぶ
                        int bestCount = -1;
                        int bestScore = int.MinValue;
                        string bestBestmove = "resign";
                        foreach (var p in bestmoveToCount)
                        {
                            if (p.Value.Count > bestCount || (p.Value.Count == bestCount && p.Value.Score > bestScore))
                            {
                                bestBestmove = p.Key;
                                bestCount = p.Value.Count;
                                bestScore = p.Value.Score;
                            }
                        }

                        // 最もスコアが高かった指し手をbestmoveとして選択する
                        // スコアが等しいものが複数ある場合はランダムに1つを選ぶ
                        // Ponderが存在するものを優先する
                        var bestmovesWithPonder = engines
                            .Select(x => x.Bestmove)
                            .Where(x => x.move == bestBestmove && x.score == bestScore && !IsNullOrEmpty(x.ponder))
                            .ToList();
                        if (bestmovesWithPonder.Count > 0)
                        {
                            bestmove = bestmovesWithPonder[random.Next(bestmovesWithPonder.Count)];
                        }
                        else
                        {
                            var bestmoves = engines
                                .Select(x => x.Bestmove)
                                .Where(x => x.move == bestBestmove && x.score == bestScore)
                                .ToList();
                            bestmove = bestmoves[random.Next(bestmoves.Count)];
                        }
                    }

                    // Apery定跡データベースを使用した場合、pv情報が不完全となる
                    // Engineクラスではこのpv情報を処理しないため、bestmove.commandがnullとなる
                    if (bestmove.command != null)
                    {
                        // PVを出力する
                        // npsは再計算する
                        long sumNps = engines
                            .Select(x => x.Bestmove)
                            .Sum(x => x.nps);
                        var commandWithNps = new List<string>(bestmove.command);
                        int sumNpsIndex = commandWithNps.IndexOf("nps");
                        if (sumNpsIndex != -1)
                        {
                            commandWithNps[sumNpsIndex + 1] = sumNps.ToString();
                        }
                        Log("<P   {0}", Join(commandWithNps));
                        WriteLineAndFlush(Console.Out, Join(commandWithNps));
                    }

                    // bestmoveを出力する
                    string outputCommand = "bestmove " + bestmove.move;
                    if (!IsNullOrEmpty(bestmove.ponder))
                    {
                        outputCommand += " ponder " + bestmove.ponder;
                    }
                    Log("<P   {0}", outputCommand);
                    WriteLineAndFlush(Console.Out, outputCommand);
                }

                UpstreamState = UpstreamStateEnum.Stopped;
                Depth = 0;
                foreach (var engine in engines)
                {
                    engine.Bestmove = null;
                }
                decideMoveLimit = DateTime.MaxValue;
                WriteToEachEngine("stop");
            }
        }

        private int GetProcessId()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return process.Id;
            }
        }

        private void CheckDecideMoveLimit()
        {
            //while (running)
            //{
            //    lock (UpstreamLockObject)
            //    {
            //        //Log("     limit={0}", decideMoveLimit.ToString("o"));
            //        if (decideMoveLimit < DateTime.Now)
            //        {
            //            DecideMove();
            //        }
            //    }
            //    Thread.Sleep(decideMoveSleepMs);
            //}
        }

        public void Run()
        {
            //writeSampleSetting();
            ProxySetting setting = loadSetting();
            string logFileFormat = Path.Combine(setting.logDirectory, string.Format(
                "tanuki-proxy.{0}.pid={1}.log.txt", DateTime.Now.ToString("yyyy-MM-dd-HH-mm-ss"),
                GetProcessId()));
            Logging.Open(logFileFormat);

            for (int id = 0; id < setting.engines.Length; ++id)
            {
                engines.Add(new Engine(this, setting.engines[id], id));
            }

            decideMoveTask = new Thread(CheckDecideMoveLimit);
            decideMoveTask.Start();

            // 子プロセスの標準入出力 (System.Diagnostics.Process) - Programming/.NET Framework/標準入出力 - 総武ソフトウェア推進所 http://smdn.jp/programming/netfx/standard_streams/1_process/
            foreach (var engine in engines)
            {
                engine.Start();
            }

            int networkDelay = 120;
            int networkDelay2 = 1120;
            int minimumThinkingTime = 2000;
            int maxGamePly = int.MaxValue;
            int time = 0;
            int byoyomi = 0;
            int inc = 0;

            Console.SetIn(new StreamReader(Console.OpenStandardInput(64 * 1024)));

            string input;
            while ((input = Console.ReadLine()) != null)
            {
                Log("U>       {0}", input);
                var command = Split(input);

                if (command[0] == "go")
                {
                    // Multi Ponder 中、 ponderhit が送られてきて、
                    // 実際には ponder がヒットしなかった場合に go コマンドを送るため、
                    // 最後に送られてきた go コマンドを保存しておく
                    lastGoCommand = new List<string>(command);

                    lock (UpstreamLockObject)
                    {
                        // 思考開始の合図です。エンジンはこれを受信すると思考を開始します。
                        int btime = ParseCommandParameter(command, "btime");
                        int wtime = ParseCommandParameter(command, "wtime");
                        time = IsBlack() ? btime : wtime;
                        byoyomi = ParseCommandParameter(command, "byoyomi");
                        int binc = ParseCommandParameter(command, "binc");
                        int winc = ParseCommandParameter(command, "winc");
                        inc = IsBlack() ? binc : winc;

                        if (!command.Contains("ponder") && !command.Contains("nodes"))
                        {
                            int maximumTime = CalculateMaximumTime(networkDelay, networkDelay2,
                                minimumThinkingTime, maxGamePly, time, byoyomi, inc, Ply());
                            decideMoveLimit = DateTime.Now.AddMilliseconds(maximumTime);
                        }

                        foreach (var engine in engines)
                        {
                            engine.Bestmove = new Engine.EngineBestmove();
                            engine.ExpectedDownstreamPosition = null;
                            engine.ExpectedDownstreamGo = null;
                        }
                        Depth = 0;
                        UpstreamState = UpstreamStateEnum.Thinking;
                        WriteLineAndFlush(Console.Out, "info string " + UpstreamPosition);
                        LastShowPv = DateTime.Now;
                    }

                    // 詰将棋ルーチンには常にgoコマンドを送信する
                    foreach (var engine in engines.Where(x => x.MateEngine))
                    {
                        engine.Write("go mate infinite");
                    }

                    foreach (var engine in engines)
                    {
                        engine.TimeKeeper = false;
                    }

                    if (command.Contains("ponder"))
                    {
                        // UpstreamPosition には相手の予想指し手を指したあとの局面がふくまれているので 1 手戻す
                        List<string> multiPonderRootPosition = Split(UpstreamPosition);
                        // 初期局面が渡されることはない
                        Debug.Assert(multiPonderRootPosition.Contains("moves"));
                        multiPonderRootPosition.RemoveAt(multiPonderRootPosition.Count - 1);

                        SearchChildNodesInParallel(multiPonderRootPosition, input);
                    }
                    else
                    {
                        GoNonPonderOrPonderhit(input);
                    }

                    // goコマンドは直接下流に渡さない
                    continue;
                }
                else if (command[0] == "isready")
                {
                    lock (UpstreamLockObject)
                    {
                        numberOfReadyoks = 0;
                    }
                }
                else if (command[0] == "position")
                {
                    lock (UpstreamLockObject)
                    {
                        UpstreamPosition = input;
                        Log("     upstreamPosition=" + UpstreamPosition);
                        // positionコマンドは直接下流に渡さない
                        // goコマンドの中で下流に渡すようにする
                        continue;
                    }
                }
                else if (command[0] == "ponderhit")
                {
                    lock (UpstreamLockObject)
                    {
                        int maximumTime = CalculateMaximumTime(networkDelay, networkDelay2,
                            minimumThinkingTime, maxGamePly, time, byoyomi, inc, Ply());
                        decideMoveLimit = DateTime.Now.AddMilliseconds(maximumTime);
                    }

                    foreach (var engine in engines)
                    {
                        engine.TimeKeeper = false;
                    }

                    GoNonPonderOrPonderhit(input);
                    // ponderhitコマンドは直接下流に渡さない
                    continue;
                }
                else if (command[0] == "stop")
                {
                    lock (UpstreamLockObject)
                    {
                        // stopが来た時に指し手を返さないと次のgoがもらえない
                        DecideMove();
                    }
                    // stopコマンドは直接下流に渡さない
                    // positionコマンドを受信した場合に止まるようにする
                    continue;
                }
                else if (command[0] == "setoption")
                {
                    lock (UpstreamLockObject)
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

        private void GoNonPonderOrPonderhit(string input)
        {
            if (engines
                .Where(x => !x.MateEngine)
                .Any(x => UpstreamPosition == x.ExpectedDownstreamPosition))
            {
                Log("Ponder Hit (*'ω'*)");
                WriteLineAndFlush(Console.Out, "info string Ponder Hit (*'ω'*)");

                // multi ponderがヒットした場合
                // ヒットしたノードにponderhitを渡し、引き続き探索させる
                var timeKeeperNode = engines
                    .Where(x => !x.MateEngine)
                    .Where(x => UpstreamPosition == x.ExpectedDownstreamPosition)
                    .First();
                timeKeeperNode.TimeKeeper = true;
                // 前回思考時に go ponder を渡していると仮定する
                timeKeeperNode.Write("ponderhit");
            }
            else
            {
                Log("ponder unhit (´・ω・｀)");
                WriteLineAndFlush(Console.Out, "info string ponder unhit (´・ω・｀)");

                // multi ponderがヒットしなかった場合
                // 最初のエンジンにrootPosを担当させる
                var timeKeeperNode = engines
                    .Where(x => !x.MateEngine)
                    .First();
                timeKeeperNode.TimeKeeper = true;
                timeKeeperNode.Write(UpstreamPosition);

                // 過去に保存しておいた go コマンドを送る
                // ponderhit が送られてきた場合、その前に送られてきた go コマンドを送る
                // ponder ではないので ponder は取り除く
                // go コマンドが送られてきた場合、そのまま送る
                List<string> goCommand = new List<string>(lastGoCommand);
                goCommand.Remove("ponder");
                timeKeeperNode.Write(Join(goCommand));
            }

            // 他のノードに子ノードを探索させる
            SearchChildNodesInParallel(Split(UpstreamPosition), "go infinite");
        }

        private void SearchChildNodesInParallel(List<string> multiPonderRootPosition, string goCommand)
        {
            if (engines
                .Where(x => !x.MateEngine)
                .Where(x => !x.TimeKeeper) // TimeKeeperノードではmultipvによる探索を行わない
                .Count() == 0)
            {
                return;
            }

            var multipvEngine = engines
                .Where(x => !x.MateEngine)
                .Where(x => !x.TimeKeeper) // TimeKeeperノードではmultipvによる探索を行わない
                .First();
            var multipv = engines
                .Where(x => !x.MateEngine)
                .Where(x => !x.TimeKeeper) // TimeKeeperノードではmultipvによる探索を行わない
                .Count();
            multipvEngine.Write(Join(multiPonderRootPosition));
            var multipvMoves = multipvEngine.GoWithMultiPv(multipv);
            var multipvEngines = engines
                .Where(x => !x.MateEngine)
                .Where(x => !x.TimeKeeper) // TimeKeeperノードではmultipvによる探索を行わない
                .ToList();
            Debug.Assert(multipvMoves.Length == multipvEngines.Count);

            string message = "Searching child positions " + Join(multipvMoves);
            Log(message);
            WriteLineAndFlush(Console.Out, "info string " + message);

            for (int i = 0; i < multipvMoves.Length; ++i)
            {
                string move = multipvMoves[i];
                // multipvの数が少なくなっている可能性があるのでnullチェックする
                if (move == null)
                {
                    continue;
                }
                var engine = multipvEngines[i];

                // 初期局面ではmovesを付ける
                if (multiPonderRootPosition.Count == 2)
                {
                    multiPonderRootPosition.Add("moves");
                }
                // 1手指した後の局面を渡す
                multiPonderRootPosition.Add(move);

                // 思考エンジンに局面を渡す
                engine.Write(Join(multiPonderRootPosition));

                // 思考エンジンにgoコマンドを渡す
                engine.Write(goCommand);

                // 1手指した後の局面を渡す
                multiPonderRootPosition.RemoveAt(multiPonderRootPosition.Count - 1);

                // 初期局面ではmovesを取り除く
                if (multiPonderRootPosition.Count == 3)
                {
                    multiPonderRootPosition.RemoveAt(multiPonderRootPosition.Count - 1);
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
        private void WriteToEachEngine(string input)
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

        /// <summary>
        /// 特定のエンジンを除くすべてのエンジンに対して出力する
        /// </summary>
        public void WriteToOtherEngines(string input, Engine own)
        {
            foreach (var engine in engines)
            {
                if (engine != own)
                {
                    engine.Write(input);
                }
            }
        }


        private bool IsBlack()
        {
            lock (UpstreamLockObject)
            {
                return Ply() % 2 == 1;
            }
        }

        /// <summary>
        /// (将棋の)開始局面からの手数を返す。平手の開始局面なら1が返る。(0ではない)
        /// </summary>
        /// <returns>開始局面からの手数</returns>
        private int Ply()
        {
            lock (UpstreamLockObject)
            {
                var position = Split(UpstreamPosition);
                int index = position.IndexOf("moves");
                if (index == -1)
                {
                    return 1;
                }
                return position.Count - index - 1;
            }
        }

        static void Main(string[] args)
        {
            using (var program = new Program())
            {
                program.Run();
            }
        }
    }
}
