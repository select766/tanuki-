using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text;
using System.Threading;
using System.IO;

namespace TanukiColiseum
{
    class Coliseum
    {
        private SemaphoreSlim GameSemaphoreSlim;
        private SemaphoreSlim FinishSemaphoreSlim = new SemaphoreSlim(0);
        public List<Game> Games { get; } = new List<Game>();
        private DateTime LastOutput = DateTime.Now;
        private const double OutputResultsPerMin = 1.0;
        private Status Status { get; } = new Status();

        public void Run(Options options)
        {
            Status.NumGames = options.NumGames;
            Status.TimeMs = options.TimeMs;

            // 定跡ファイルの読み込み
            string[] book = File.ReadAllLines(options.SfenFilePath);

            Console.WriteLine("Initializing engines...");
            Console.Out.Flush();

            for (int gameIndex = 0; gameIndex < options.NumConcurrentGames; ++gameIndex)
            {
                int numaNode = gameIndex * options.NumNumaNodes / options.NumConcurrentGames;

                // エンジン1初期化
                var options1 = new List<string>();
                options1.Add("setoption name Threads value 1");
                options1.Add("setoption name EvalDir value " + options.Eval1FolderPath);
                options1.Add("setoption name Hash value " + options.HashMb);
                options1.Add("setoption name BookFile value no_book");
                options1.Add("setoption MinimumThinkingTime value 1000");
                options1.Add("setoption name NetworkDelay value 0");
                options1.Add("setoption name NetworkDelay2 value 0");
                options1.Add("setoption name EvalShare value true");
                options1.Add("setoption name BookMoves value " + options.NumBookMoves1);
                options1.Add("setoption name BookFile value " + options.BookFileName1);
                Console.WriteLine("Starting the engine process " + (gameIndex * 2));
                Console.Out.Flush();
                var engine1 = new Engine(options.Engine1FilePath, options1, this, gameIndex * 2, gameIndex, 0, numaNode);
                engine1.StartAsync().Wait();

                // エンジン2初期化
                var options2 = new List<string>();
                options2.Add("setoption name Threads value 1");
                options2.Add("setoption name EvalDir value " + options.Eval2FolderPath);
                options2.Add("setoption name Hash value " + options.HashMb);
                options2.Add("setoption name BookFile value no_book");
                options2.Add("setoption MinimumThinkingTime value 1000");
                options2.Add("setoption name NetworkDelay value 0");
                options2.Add("setoption name NetworkDelay2 value 0");
                options2.Add("setoption name EvalShare value true");
                options2.Add("setoption name BookMoves value " + options.NumBookMoves2);
                options2.Add("setoption name BookFile value " + options.BookFileName2);
                Console.WriteLine("Starting the engine process " + (gameIndex * 2 + 1));
                Console.Out.Flush();
                var engine2 = new Engine(options.Engine2FilePath, options2, this, gameIndex * 2 + 1, gameIndex, 1, numaNode);
                engine2.StartAsync().Wait();

                // ゲーム初期化
                // 偶数番目はengine1が先手、奇数番目はengine2が先手
                Games.Add(new Game(gameIndex & 1, options.TimeMs, engine1, engine2, options.NumBookMoves));
            }

            Console.WriteLine("Initialized engines...");
            Console.WriteLine("Started games...");
            Console.Out.Flush();

            // numConcurrentGames局同時に対局できるようにする
            GameSemaphoreSlim = new SemaphoreSlim(options.NumConcurrentGames, options.NumConcurrentGames);
            var random = new Random();
            for (int i = 0; i < options.NumGames; ++i)
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

            Console.WriteLine("engine1={0} eval1={1}", options.Engine1FilePath, options.Eval1FolderPath);
            Console.WriteLine("engine2={0} eval2={1}", options.Engine2FilePath, options.Eval2FolderPath);
            OutputResult();
        }

        public void OnGameFinished(int enginWin, int blackWhiteWin, bool draw)
        {
            if (!draw)
            {
                Interlocked.Increment(ref Status.Win[enginWin, blackWhiteWin]);
            }
            else
            {
                Interlocked.Increment(ref Status.NumDraw);
            }

            if (LastOutput.AddMinutes(OutputResultsPerMin) < DateTime.Now)
            {
                OutputResult();
                LastOutput = DateTime.Now;
            }
            GameSemaphoreSlim.Release();
            FinishSemaphoreSlim.Release();
        }

        private void OutputResult()
        {
            var status = new Status(Status);

            int engine1Win = status.Win[0, 0] + status.Win[0, 1];
            int engine2Win = status.Win[1, 0] + status.Win[1, 1];
            int draw = status.NumDraw;
            int blackWin = status.Win[0, 0] + status.Win[1, 0];
            int whiteWin = status.Win[0, 1] + status.Win[1, 1];

            // T1,b10000,433 - 54 - 503(46.26% R-26.03) win black : white = 52.42% : 47.58%
            double winRate = engine1Win / (double)(engine1Win + engine2Win);
            double rating = 0.0;
            if (1e-8 < winRate && winRate < 1.0 - 1e-8)
            {
                rating = -400.0 * Math.Log10((1.0 - winRate) / winRate);
            }
            double black = blackWin / (double)(blackWin + whiteWin);
            double white = whiteWin / (double)(blackWin + whiteWin);
            Console.WriteLine("T1,b{0},{1} - {2} - {3}({4:0.00%} R{5:0.00}) win black: white = {6:0.00%} : {7:0.00%}",
                status.TimeMs, engine1Win, draw, engine2Win, winRate, rating, black, white);
            Console.Out.Flush();
        }
    }
}
