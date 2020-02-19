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
                    WriteToUI("bestmove resign");
                }
                else if (engines.Select(x => x.Bestmove).Any(x => x.move == "resign"))
                {
                    // 投了
                    WriteToUI("bestmove resign");

                }
                else if (engines.Select(x => x.Bestmove).Any(x => x.move == "win"))
                {
                    //宣言勝ち
                    WriteToUI("bestmove win");
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
                    if (bestmove.pvCommand != null)
                    {
                        // PVを出力する
                        // npsは再計算する
                        long sumNps = engines
                            .Select(x => x.Bestmove)
                            .Sum(x => x.nps);
                        var commandWithNps = new List<string>(bestmove.pvCommand);
                        int sumNpsIndex = commandWithNps.IndexOf("nps");
                        if (sumNpsIndex != -1)
                        {
                            commandWithNps[sumNpsIndex + 1] = sumNps.ToString();
                        }
                        WriteToUI(Join(commandWithNps));
                    }

                    // TODO(hnoda): 詰将棋専用エンジンが返してきた手順をinfo pvで出力する

                    // bestmoveを出力する
                    string outputCommand = "bestmove " + bestmove.move;
                    if (!IsNullOrEmpty(bestmove.ponder))
                    {
                        outputCommand += " ponder " + bestmove.ponder;
                    }
                    WriteToUI(outputCommand);
                }

                UpstreamState = UpstreamStateEnum.Stopped;
                Depth = 0;
                foreach (var engine in engines)
                {
                    // 本当はnullを代入したいが、思考エンジンがbesmoveを返してtanuki-proxyがbestmoveを返したあと、
                    // stopコマンドを受信した場合、Bestmoveにアクセスして落ちるのを防ぐ
                    engine.Bestmove = null;
                }
                decideMoveLimit = DateTime.MaxValue;

                // ponderhit時に施行を継続するため、
                // stopコマンドを送らないようにする。
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
            List<string> lastGoCommand = null;

            Console.SetIn(new StreamReader(Console.OpenStandardInput(64 * 1024)));

            string input;
            while ((input = Console.ReadLine()) != null)
            {
                Log("U>       {0}", input);
                var command = Split(input);

                if (command[0] == "go")
                {
                    // 持ち時間0、1手毎の加算秒数を正に設定すると、初手の思考時間が0となる。
                    // これにより初手の指し手が狂ってしまう。
                    // これを避けるため、btimeとwtimeの値を補正する。
                    int btimeIndex = command.IndexOf("btime");
                    if (btimeIndex != -1 && command[btimeIndex + 1] == "0")
                    {
                        command[btimeIndex + 1] = "1000";
                    }

                    int wtimeIndex = command.IndexOf("wtime");
                    if (wtimeIndex != -1 && command[wtimeIndex + 1] == "0")
                    {
                        command[wtimeIndex + 1] = "1000";
                    }

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
                        WriteInfoStringToUI(UpstreamPosition);
                        LastShowPv = DateTime.Now;
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

                        // ponderの指し手に多くの思考エンジンを割り当てるため、ここで保存しておく。
                        string ponder = multiPonderRootPosition[multiPonderRootPosition.Count - 1];

                        // ponderの指し手を取り除く。
                        multiPonderRootPosition.RemoveAt(multiPonderRootPosition.Count - 1);

                        SearchDescendentNodesInParallel(multiPonderRootPosition, lastGoCommand, new HashSet<Engine>(), ponder, 0);
                    }
                    else
                    {
                        GoNonPonderOrPonderhit(lastGoCommand);
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

                    GoNonPonderOrPonderhit(lastGoCommand);
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

        /// <summary>
        /// ponderなしのgoコマンドやponderhitコマンドを処理する。
        /// </summary>
        /// <param name="lastGoCommand">最後に受信したgoコマンド</param>
        private void GoNonPonderOrPonderhit(List<string> lastGoCommand)
        {
            var assignedEngines = new HashSet<Engine>();

            if (engines
                .Where(x => !x.MateEngine)
                .Any(x => UpstreamPosition == x.ExpectedDownstreamPosition))
            {
                // multi ponderがヒットした場合
                // ヒットしたノードにponderhitを渡し、引き続き探索させる
                var timeKeeperNode = engines
                    .Where(x => !x.MateEngine)
                    .Where(x => UpstreamPosition == x.ExpectedDownstreamPosition)
                    .First();
                timeKeeperNode.TimeKeeper = true;
                // 前回思考時に go ponder を渡していると仮定する
                timeKeeperNode.Write("ponderhit");

                // 詰将棋専用エンジンに局面の探索をさせる。
                var mateEngines = engines
                    .Where(x => x.MateEngine)
                    .ToList();
                if (mateEngines.Count > 0)
                {
                    mateEngines[0].Write(UpstreamPosition);
                    mateEngines[0].Write("go mate infinite");
                }

                int engineIndex = engines.IndexOf(timeKeeperNode);
                WriteInfoStringToUI($"Ponder Hit (^_^) engineIndex={engineIndex}");

                assignedEngines.Add(timeKeeperNode);
            }
            else
            {
                WriteInfoStringToUI("ponder unhit (-_-)");

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
                var goCommand = new List<string>(lastGoCommand);
                goCommand.Remove("ponder");
                timeKeeperNode.Write(Join(goCommand));

                // 詰将棋専用エンジンに局面の探索をさせる。
                var mateEngines = engines
                    .Where(x => x.MateEngine)
                    .ToList();
                if (mateEngines.Count > 0)
                {
                    mateEngines[0].Write(UpstreamPosition);
                    mateEngines[0].Write("go mate infinite");
                }

                assignedEngines.Add(timeKeeperNode);
            }

            // 他のノードに子ノードを探索させる
            SearchDescendentNodesInParallel(Split(UpstreamPosition), lastGoCommand, assignedEngines, null, 1);
        }

        private class PositionAndNumAssignedNodes
        {
            public List<string> Position { get; set; }
            public int NumAssignedEngines { get; set; }
        }


        /// <summary>
        /// 指定された局面の子孫局面を複数のノードで探索する。
        /// </summary>
        /// <param name="multiPonderRootPosition">マルチponderを行うにあたってのRoot局面。この局面の子孫局面を思考エンジンに割り当てていく。</param>
        /// <param name="lastGoCommand">最後に受信したgoコマンド</param>
        /// <param name="assignedEngines">既に探索が始まっている思考エンジン</param>
        /// <param name="ponder">ponderで指定された指し手。指し手がない場合はnull。</param>
        /// <param name="nextMateEngineIndex">次に使用する思考エンジンのindex</param>
        private void SearchDescendentNodesInParallel(
            List<string> multiPonderRootPosition, List<string> lastGoCommand,
            HashSet<Engine> assignedEngines, string ponder, int nextMateEngineIndex)
        {
            if (engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                .Count() == 0)
            {
                return;
            }

            var mateEngines = engines
                .Where(x => x.MateEngine)
                .ToList();
            int mateEngineIndex = 1;

            // 幅優先探索でMultiPoonderの探索対象局面を割り当てていく。

            // MultiPonderの開始局面をキューに入れる。
            var queue = new Queue<PositionAndNumAssignedNodes>();
            queue.Enqueue(new PositionAndNumAssignedNodes
            {
                Position = multiPonderRootPosition,
                NumAssignedEngines = engines
                    .Where(x => !x.MateEngine)
                    .Where(x => !assignedEngines.Contains(x))
                    .Count()
            });

            while (queue.Count() > 0)
            {
                var positionAndNumAssignedNodes = queue.Dequeue();
                var position = positionAndNumAssignedNodes.Position;
                var numAssignedNodes = positionAndNumAssignedNodes.NumAssignedEngines;

                // MultiPV探索に使用するノードを選ぶ。
                var multiPVEngine = FindAvailaleEngineForMultiPVSearch(assignedEngines, multiPonderRootPosition);

                // いくつの指し手を出力させるか決める。
                // TODO(hnoda): 計算式を決める。指し手の幅を大きくしたほうが良いか…？
                int multiPV = 0;
                for (int tempNumAssignedNodes = numAssignedNodes; tempNumAssignedNodes > 0; tempNumAssignedNodes -= (tempNumAssignedNodes + 1) / 2)
                {
                    ++multiPV;
                }

                WriteInfoStringToUI($"Searching child positions. position={Join(position).Substring(Join(multiPonderRootPosition).Length).Trim()}");
                var multiPVMoves = multiPVEngine.SearchWithMultiPv(Join(position), multiPV);

                // ponderの指し手が存在していた場合、その指し手を先頭に持ってくる。
                // この処理は、root局面の子局面についてのみ行われる。
                if (ponder != null)
                {
                    // ponderとmultiPVMovesを結合する。
                    multiPVMoves = new string[] { ponder }
                        .Concat(multiPVMoves)
                        // 重複した指し手を取り除く。
                        .Distinct()
                        .Where(x => x != null)
                        .ToArray();

                    // 先頭のmultiPV個を選ぶ。
                    if (multiPVMoves.Length > multiPV)
                    {
                        multiPVMoves = multiPVMoves
                            .Take(multiPV)
                            .ToArray();
                    }

                    // 合法手がmultiPV未満の場合、multiPVMovesの数がmultiPV以下になっている点に注意する。

                    // 他の局面でponderの値が使用されないよう、nullを代入しておく。
                    ponder = null;
                }

                int numRemainedNodes = numAssignedNodes;
                foreach (var multiPVMove in multiPVMoves)
                {
                    // multipvの数が少なくなっている可能性があるのでnullチェックする。
                    if (multiPVMove == null)
                    {
                        // 探索する指し手が無かった。
                        // 思考エンジンを遊ばせておく。
                        continue;
                    }

                    var targetPosition = AddMove(position, multiPVMove);
                    var assignedEngine = FindAvailaleEngineForSearch(assignedEngines, multiPonderRootPosition, Join(targetPosition));

                    if (assignedEngine.ExpectedDownstreamPosition != Join(targetPosition))
                    {
                        // ExpectedDownstreamPositionがtargetPositionと等しい場合、
                        // go ponderによりすでに局面の探索が行われていることを表す。
                        // その場合は思考エンジンに対してコマンドは送らない。
                        // それ以外の場合は思考エンジンに局面を渡し、goコマンドを送る。
                        assignedEngine.Write(Join(targetPosition));

                        // 思考エンジンに局面の探索を開始させる。
                        // ponderヒット時にponderhitコマンドで即指すことができるよう、
                        // go ponderコマンドを送信する。
                        // go ponderコマンドの引数は、最後に受信したgoコマンドとする。
                        var goPonderCommand = new List<string>(lastGoCommand);
                        Debug.Assert(goPonderCommand[0] == "go");

                        // ponderが含まれていない場合は追加する。
                        // TODO(hnoda): Pre-ponderヒット時等、残り時間が実際の値と異なる場合がある。
                        //              これにより、時間切れ等が起こる可能性がある。
                        if (!goPonderCommand.Contains("ponder"))
                        {
                            goPonderCommand.Insert(1, "ponder");
                        }

                        // 一つ前の手で長く思考した場合、残り時間が少なくなっている場合がある。
                        // このとき、前の手と同じ残り時間のつもりで思考すると時間切れとなる場合がある。
                        // これを防ぐため、思考時間を短くする。
                        if (goPonderCommand.Contains("inc"))
                        {
                            int incIndex = goPonderCommand.IndexOf("inc");
                            int btimeIndex = goPonderCommand.IndexOf("btime");
                            if (btimeIndex != -1)
                            {
                                goPonderCommand[btimeIndex + 1] = goPonderCommand[incIndex + 1];
                            }
                            int wtimeIndex = goPonderCommand.IndexOf("wtime");
                            if (wtimeIndex != -1)
                            {
                                goPonderCommand[wtimeIndex + 1] = goPonderCommand[incIndex + 1];
                            }
                        }

                        // 思考エンジンにgo ponderコマンドを送信する。
                        assignedEngine.Write(Join(goPonderCommand));

                        int engineIndex = engines.IndexOf(assignedEngine);
                        WriteInfoStringToUI($"Assigned a position. engineIndex={engineIndex} position={Join(targetPosition).Substring(Join(multiPonderRootPosition).Length).Trim()}");
                    }
                    else
                    {
                        int engineIndex = engines.IndexOf(assignedEngine);
                        WriteInfoStringToUI($"Reused an engine. engineIndex={engineIndex} position={Join(targetPosition).Substring(Join(multiPonderRootPosition).Length).Trim()}");
                    }

                    assignedEngines.Add(assignedEngine);

                    if (mateEngineIndex < mateEngines.Count)
                    {
                        // 詰将棋エンジンに局面を渡す。
                        var mateEngine = mateEngines[mateEngineIndex++];
                        mateEngine.Write(Join(targetPosition));

                        // 詰将棋エンジンにgo mate infiniteコマンドを渡す。
                        mateEngine.Write("go mate inifinite");
                    }

                    // この局面の子孫局面に割り当てられる思考エンジンの数。
                    // この局面を探索する思考エンジンは除く。
                    int nextNumAssignedNodes = (numRemainedNodes + 1) / 2 - 1;
                    // 現局面の探索に思考エンジンを一つ使っているので、1足す。
                    numRemainedNodes -= nextNumAssignedNodes + 1;
                    if (nextNumAssignedNodes > 0)
                    {
                        queue.Enqueue(new PositionAndNumAssignedNodes
                        {
                            Position = targetPosition,
                            NumAssignedEngines = nextNumAssignedNodes,
                        });
                    }
                }

                // MultiPV探索で得られた指し手が足りない場合、numRemainedNodesが1以上になる場合がある。
            }
        }

        /// <summary>
        /// 対象の局面の探索に使用する探索エンジンを一つ選ぶ
        /// </summary>
        /// <param name="assignedEngines"></param>
        /// <param name="multiPonderRootPosition"></param>
        /// <param name="targetPosition">指定されていた場合、この局面を探索中の思考エンジンを優先して返す。</param>
        /// <returns></returns>
        private Engine FindAvailaleEngineForSearch(HashSet<Engine> assignedEngines, List<string> multiPonderRootPosition, string targetPosition)
        {
            // targetPositionを探索中の思考エンジンがある場合、それを返す。
            var engine = engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                .Where(x => x.ExpectedDownstreamPosition == targetPosition)
                // 対局開始直後はExpectedDownstreamPositionが全て空になる。
                // そのためSingleOrDefault()を使用すると例外が飛んでしまう。
                // これを避けるためにFirstOrDefault()を使用する。
                .FirstOrDefault();
            if (engine != null)
            {
                return engine;
            }

            // 見つからなかったので、Root局面以下を探索している思考エンジンを選ぶ。
            return engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                .First();
        }

        /// <summary>
        /// Multi PV探索に使用する探索エンジンを一つ選ぶ
        /// </summary>
        /// <param name="assignedEngines"></param>
        /// <param name="multiPonderRootPosition"></param>
        /// <returns></returns>
        private Engine FindAvailaleEngineForMultiPVSearch(HashSet<Engine> assignedEngines, List<string> multiPonderRootPosition)
        {
            // 自分の手番中は、root局面==multiPonderRootPositionとなり、探索中である。
            // 相手の手番中は、multiPonderRootPositionを探索していた思考エンジンは、局面を思考していない。
            // multiPonderRootPositionを探索している局面を優先して返す。
            var engine = engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                // 相手の手番中、multiPonderRootPositionを探索している思考エンジンを優先して返す。
                .Where(x => x.ExpectedDownstreamPosition == Join(multiPonderRootPosition))
                .FirstOrDefault();
            if (engine != null)
            {
                return engine;
            }

            // なるべく現在のRoot局面以下以外を探索している思考エンジンを選ぶ。
            engine = engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                // 開始直後はx.ExpectedDownstreamPositionがnullとなっている点に注意する。
                .Where(x => x.ExpectedDownstreamPosition != null && !x.ExpectedDownstreamPosition.StartsWith(Join(multiPonderRootPosition)))
                .FirstOrDefault();
            if (engine != null)
            {
                return engine;
            }

            // 見つからなかったので、Root局面以下を探索している思考エンジンを選ぶ。
            return engines
                .Where(x => !x.MateEngine)
                .Where(x => !assignedEngines.Contains(x))
                // エンジン番号の小さい思考エンジンは、rootに近い局面を探索している可能性が高い。
                // これをMulti PV探索に使用すると若干損である。
                // これを防ぐためエンジン番号の大きいエンジンを優先する。
                .Last();
        }

        /// <summary>
        /// positionコマンド列に指し手を追加する。
        /// </summary>
        /// <param name="position"></param>
        /// <param name="move"></param>
        /// <returns></returns>
        private List<string> AddMove(List<string> position, string move)
        {
            var targetPosition = new List<string>(position);

            // 初期局面ではmovesを付ける
            if (targetPosition.Count == 2)
            {
                targetPosition.Add("moves");
            }

            targetPosition.Add(move);
            return targetPosition;
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
                WriteInfoStringToUI("max_game_ply is too small.");
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
            WriteInfoStringToUI($"maximumTime={maximumTime}");
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
        /// 特定のエンジンと詰将棋エンジンを除くすべてのエンジンに対して出力する
        /// </summary>
        public void WriteToOtherEngines(string input, Engine own)
        {
            foreach (var engine in engines.Where(x => !x.MateEngine))
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

        private static void WriteInfoStringToUI(string infoString)
        {
            WriteToUI($"info string {infoString}");
        }

        private static void WriteToUI(string usiCommand)
        {
            Log($"<P   {usiCommand}");
            WriteLineAndFlush(Console.Out, $"{usiCommand}");
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
