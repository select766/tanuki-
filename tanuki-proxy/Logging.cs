using System;
using System.Collections.Concurrent;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Timers;
using static System.String;

namespace tanuki_proxy
{
    public class Logging
    {
        private static object LockObject = new object();
        private static TextWriter TextWriter;
        private static System.Timers.Timer FlushTimer;
        private static BlockingCollection<string> Queue;
        public static void Open(string filePath)
        {
            lock (LockObject)
            {
                TextWriter = new StreamWriter(filePath, false, Encoding.UTF8);
                Queue = new BlockingCollection<string>();
                FlushTimer = new System.Timers.Timer(1000);
                FlushTimer.Elapsed += FlushTimerCallback;
                FlushTimer.Start();

                Task.Run(() =>
                {
                    var queue = Queue;
                    while (!queue.IsCompleted)
                    {
                        string line = null;
                        try
                        {
                            line = Queue.Take();
                        }
                        catch (InvalidOperationException)
                        {
                            continue;
                        }

                        if (string.IsNullOrEmpty(line))
                        {
                            continue;
                        }

                        lock (LockObject)
                        {
                            TextWriter.WriteLine(line);
                        }
                    }
                });
            }
        }

        private static void FlushTimerCallback(object sender, ElapsedEventArgs e)
        {
            lock (LockObject)
            {
                TextWriter.Flush();
            }
        }

        public static void Close()
        {
            lock (LockObject)
            {
                FlushTimer.Stop();
                FlushTimer = null;

                TextWriter.Close();
                TextWriter = null;

                var queue = Queue;
                Queue = null;
                queue.CompleteAdding();
            }
        }

        public static void Log(string format, object arg0)
        {
            Queue?.Add(string.Format(DateTime.Now.ToString("o") + Format(" ({0,2}) ", Thread.CurrentThread.ManagedThreadId) + format, arg0));
        }

        public static void Log(string format, object arg0, object arg1)
        {
            Queue?.Add(string.Format(DateTime.Now.ToString("o") + Format(" ({0,2}) ", Thread.CurrentThread.ManagedThreadId) + format, arg0, arg1));
        }

        public static void Log(string format, object arg0, object arg1, object arg2)
        {
            Queue?.Add(string.Format(DateTime.Now.ToString("o") + Format(" ({0,2}) ", Thread.CurrentThread.ManagedThreadId) + format, arg0, arg1, arg2));
        }

        public static void Log(string format, params object[] arg)
        {
            Queue?.Add(string.Format(DateTime.Now.ToString("o") + Format(" ({0,2}) ", Thread.CurrentThread.ManagedThreadId) + format, arg));
        }
    }
}
