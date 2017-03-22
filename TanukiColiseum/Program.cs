using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.Threading;
using System.Text.RegularExpressions;
using System.IO;

namespace TanukiColiseum
{
    class Util
    {
        public static List<string> Split(string s)
        {
            return new List<string>(new Regex("\\s+").Split(s));
        }
    }

    class Engine
    {
        private const int MaxMoves = 256;
        private Process Process = new Process();
        private Program Program;
        private List<string> Options;
        private SemaphoreSlim ReadyokSemaphoreSlim = new SemaphoreSlim(0);
        private int ProcessIndex;
        private int GameIndex;
        private int EngineIndex;

        public Engine(string fileName, List<string> options, Program program, int processIndex, int gameIndex, int engineIndex)
        {
            this.Process.StartInfo.FileName = fileName;
            this.Process.StartInfo.UseShellExecute = false;
            this.Process.StartInfo.RedirectStandardInput = true;
            this.Process.StartInfo.RedirectStandardOutput = true;
            this.Process.StartInfo.RedirectStandardError = true;
            this.Process.OutputDataReceived += HandleStdout;
            this.Process.ErrorDataReceived += HandleStderr;
            this.Program = program;
            this.Options = options;
            this.ProcessIndex = processIndex;
            this.GameIndex = gameIndex;
            this.EngineIndex = engineIndex;
        }

        /// <summary>
        /// 思考エンジンを開始し、isreadyを送信し、readyokが返るのを待つ
        /// </summary>
        /// <returns></returns>
        public async Task StartAsync()
        {
            //BeginOutputReadLine()/BeginErrorReadLine()が呼び出されたあと
            //読み込みスレッドが動かないので
            // スレッド経由で実行する
            // TODO(nodchip): async/await構文に書き換える
            Process.Start();
            Process.BeginOutputReadLine();
            Process.BeginErrorReadLine();
            Send("usi");
            foreach (var option in Options)
            {
                Send(option);
            }
            Send("isready");
            await ReadyokSemaphoreSlim.WaitAsync().ConfigureAwait(false);
        }

        public void Finish()
        {
            Send("quit");
            Process.WaitForExit();
            Process.Dispose();
        }

        /// <summary>
        /// エンジンにコマンドを送信する
        /// </summary>
        /// <param name="command"></param>
        public void Send(string command)
        {
            //Debug.WriteLine("    > [{0}] {1}", ProcessIndex, command);
            Process.StandardInput.WriteLine(command);
            Process.StandardInput.Flush();
        }

        /// <summary>
        /// 思考エンジンの標準出力を処理する
        /// </summary>
        /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
        /// <param name="e">思考エンジンの出力</param>
        private void HandleStdout(object sender, DataReceivedEventArgs e)
        {
            if (e.Data == null)
            {
                return;
            }

            //Debug.WriteLine("    < [{0}] {1}", ProcessIndex, e.Data);

            List<string> command = Util.Split(e.Data);
            if (command.Contains("readyok"))
            {
                HandleReadyok(command);
            }
            else if (command.Contains("bestmove"))
            {
                HandleBestmove(command);
            }
        }

        /// <summary>
        /// 思考エンジンの標準エラー出力を処理する
        /// </summary>
        /// <param name="sender">出力を送ってきた思考エンジンのプロセス</param>
        /// <param name="e">思考エンジンの出力</param>
        private void HandleStderr(object sender, DataReceivedEventArgs e)
        {
            Debug.WriteLine("    ! [{0}] {1}", ProcessIndex, e.Data);
        }

        private void HandleReadyok(List<string> command)
        {
            // readyokが返ってきたのでセマフォを開放してStart()関数から抜けさせる
            ReadyokSemaphoreSlim.Release();
        }

        private void HandleBestmove(List<string> command)
        {
            var game = Program.Games[GameIndex];
            if (command[1] == "resign" || command[1] == "win" || game.Moves.Count >= MaxMoves)
            {
                if (command[1] == "resign")
                {
                    // 相手側の勝数を上げる
                    Interlocked.Increment(ref Program.Engine12Win[game.Turn ^ 1]);
                    Interlocked.Increment(ref Program.BlackWhiteWin[(game.Moves.Count + 1) & 1]);
                }
                else if (command[1] == "win")
                {
                    // 自分側の勝数を上げる
                    Interlocked.Increment(ref Program.Engine12Win[game.Turn]);
                    Interlocked.Increment(ref Program.BlackWhiteWin[game.Moves.Count & 1]);
                }
                else
                {
                    // 引き分け
                    Interlocked.Increment(ref Program.Draw);
                }

                // 次の対局を開始する
                // 先にGame.OnGameFinished()を読んでゲームの状態を停止状態に移行する
                game.OnGameFinished();
                Program.OnGameFinished();
            }
            else
            {
                game.OnMove(command[1]);
                game.Go();
            }
        }
    }

    class Game
    {
        private const int NumBookMoves = 24;
        public List<string> Moves { get; } = new List<string>();
        public int Turn { get; private set; }
        public int InitialTurn { get; private set; }
        public bool Running { get; set; } = false;
        public List<Engine> Engines { get; } = new List<Engine>();
        private int TimeMs;
        private Random Random = new Random();

        public Game(int initialTurn, int timeMs, Engine engine1, Engine engine2)
        {
            this.TimeMs = timeMs;
            this.Engines.Add(engine1);
            this.Engines.Add(engine2);
        }

        public void OnNewGame(string sfen)
        {
            Moves.Clear();
            int numBookMoves = Random.Next(NumBookMoves);
            foreach (var move in Util.Split(sfen))
            {
                if (move == "startpos" || move == "moves")
                {
                    continue;
                }
                Moves.Add(move);
                if (Moves.Count >= numBookMoves)
                {
                    break;
                }
            }

            Turn = InitialTurn;

            Running = true;

            foreach (var engine in Engines)
            {
                engine.Send("usinewgame");
            }
        }

        public void Go()
        {
            string command = "position startpos moves";
            foreach (var move in Moves)
            {
                command += " ";
                command += move;
            }
            Engines[Turn].Send(command);
            Engines[Turn].Send(string.Format("go byoyomi {0}", TimeMs));
        }
        public void OnMove(string move)
        {
            Moves.Add(move);
            Turn ^= 1;
        }

        public void OnGameFinished()
        {
            InitialTurn ^= 1;
            Running = false;
        }
    }

    class Program
    {
        private const string BookFilePath = @"book\records2016_10818.sfen";
        //private const string BookFilePath = @"book.sfen";
        private SemaphoreSlim GameSemaphoreSlim;
        private SemaphoreSlim FinishSemaphoreSlim = new SemaphoreSlim(0);
        public List<Game> Games { get; } = new List<Game>();
        public int[] Engine12Win { get; } = { 0, 0 };
        public int Draw = 0;
        public int[] BlackWhiteWin { get; } = { 0, 0 };

        public void Run(string[] args)
        {
            string engine1FilePath = null;
            string engine2FilePath = null;
            string eval1FolderPath = null;
            string eval2FolderPath = null;
            int numConcurrentGames = 0;
            int numGames = 0;
            int hashMb = 0;
            int timeMs = 0;

            for (int i = 0; i < args.Length; ++i)
            {
                switch (args[i])
                {
                    case "--engine1":
                        engine1FilePath = args[++i];
                        break;
                    case "--engine2":
                        engine2FilePath = args[++i];
                        break;
                    case "--eval1":
                        eval1FolderPath = args[++i];
                        break;
                    case "--eval2":
                        eval2FolderPath = args[++i];
                        break;
                    case "--num_concurrent_games":
                        numConcurrentGames = int.Parse(args[++i]);
                        break;
                    case "--num_games":
                        numGames = int.Parse(args[++i]);
                        break;
                    case "--hash":
                        hashMb = int.Parse(args[++i]);
                        break;
                    case "--time":
                        timeMs = int.Parse(args[++i]);
                        break;
                    default:
                        throw new Exception("Unexpected option: " + args[i]);
                }
            }

            if (engine1FilePath == null)
            {
                throw new Exception("--engine1 is not specified.");
            }
            else if (engine2FilePath == null)
            {
                throw new Exception("--engine2 is not specified.");
            }
            else if (eval1FolderPath == null)
            {
                throw new Exception("--eval1 is not specified.");
            }
            else if (eval2FolderPath == null)
            {
                throw new Exception("--eval2 is not specified.");
            }
            else if (numConcurrentGames == 0)
            {
                throw new Exception("--num_concurrent_games is not specified.");
            }
            else if (numGames == 0)
            {
                throw new Exception("--num_games is not specified.");
            }
            else if (hashMb == 0)
            {
                throw new Exception("--hash is not specified.");
            }
            else if (timeMs == 0)
            {
                throw new Exception("--time is not specified.");
            }

            // 定跡ファイルの読み込み
            string[] book = File.ReadAllLines(BookFilePath);

            for (int gameIndex = 0; gameIndex < numConcurrentGames; ++gameIndex)
            {
                // エンジン1初期化
                var options1 = new List<string>();
                options1.Add("setoption name Threads value 1");
                options1.Add("setoption name EvalDir value " + eval1FolderPath);
                options1.Add("setoption name Hash value " + hashMb);
                options1.Add("setoption name BookFile value no_book");
                options1.Add("setoption MinimumThinkingTime value 1000");
                options1.Add("setoption name NetworkDelay value 0");
                options1.Add("setoption name NetworkDelay2 value 0");
                options1.Add("setoption name EvalShare value true");
                Debug.WriteLine("Starting the engine process " + (gameIndex * 2));
                var engine1 = new Engine(engine1FilePath, options1, this, gameIndex * 2, gameIndex, 0);
                engine1.StartAsync().Wait();
                Debug.WriteLine("Started the engine process " + (gameIndex * 2));

                // エンジン2初期化
                var options2 = new List<string>();
                options2.Add("setoption name Threads value 1");
                options2.Add("setoption name EvalDir value " + eval2FolderPath);
                options2.Add("setoption name Hash value " + hashMb);
                options2.Add("setoption name BookFile value no_book");
                options2.Add("setoption MinimumThinkingTime value 1000");
                options2.Add("setoption name NetworkDelay value 0");
                options2.Add("setoption name NetworkDelay2 value 0");
                options2.Add("setoption name EvalShare value true");
                Debug.WriteLine("Starting the engine process " + (gameIndex * 2 + 1));
                var engine2 = new Engine(engine1FilePath, options1, this, gameIndex * 2 + 1, gameIndex, 1);
                engine2.StartAsync().Wait();
                Debug.WriteLine("Started the engine process " + (gameIndex * 2 + 1));

                // ゲーム初期化
                // 偶数番目はengine1が先手、奇数番目はengine2が先手
                Games.Add(new Game(gameIndex & 1, timeMs, engine1, engine2));
            }

            // numConcurrentGames局同時に対局できるようにする
            GameSemaphoreSlim = new SemaphoreSlim(numConcurrentGames, numConcurrentGames);
            var random = new Random();
            for (int i = 0; i < numGames; ++i)
            {
                GameSemaphoreSlim.Wait();

                // 空いているゲームインスタンスを探す
                Game game = Games.Find(x => !x.Running);
                game.OnNewGame(book[random.Next(book.Length)]);
                game.Go();
            }

            while (Games.Count(game => game.Running) > 0)
            {
                FinishSemaphoreSlim.Wait();
            }

            foreach (var game in Games)
            {
                foreach (var engine in game.Engines)
                {
                    engine.Finish();
                }
            }

            Console.WriteLine("win={0} draw={1} lose={2} black={3} white={4}", Engine12Win[0], Draw, Engine12Win[1], BlackWhiteWin[0], BlackWhiteWin[1]);
        }

        public void OnGameFinished()
        {
            Console.WriteLine("win={0} draw={1} lose={2} black={3} white={4}", Engine12Win[0], Draw, Engine12Win[1], BlackWhiteWin[0], BlackWhiteWin[1]);
            GameSemaphoreSlim.Release();
            FinishSemaphoreSlim.Release();
        }

        static void Main(string[] args)
        {
            new Program().Run(args);
        }
    }
}
