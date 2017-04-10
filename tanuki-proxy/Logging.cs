using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Threading;

namespace tanuki_proxy
{
    public class Logging
    {
        private static object LockObject = new object();
        private static TextWriter TextWriter;
        private static Timer Timer;
        public static void Open(string filePath)
        {
            lock (LockObject)
            {
                TextWriter = new StreamWriter(filePath, false, Encoding.UTF8);
                Timer = new Timer(new TimerCallback(Callback));
                Timer.Change(1000, 1000);
            }
        }

        private static void Callback(object args)
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
                Timer.Change(Timeout.Infinite, Timeout.Infinite);
                Timer = null;
                TextWriter.Close();
                TextWriter = null;
            }
        }

        public static void Log(string format, object arg0)
        {
            lock (LockObject)
            {
                TextWriter?.WriteLine(format, arg0);
            }
        }

        public static void Log(string format, object arg0, object arg1)
        {
            lock (LockObject)
            {
                TextWriter?.WriteLine(format, arg0, arg1);
            }
        }

        public static void Log(string format, object arg0, object arg1, object arg2)
        {
            lock (LockObject)
            {
                TextWriter?.WriteLine(format, arg0, arg1, arg2);
            }
        }

        public static void Log(string format, params object[] arg)
        {
            lock (LockObject)
            {
                TextWriter?.WriteLine(format, arg);
            }
        }
    }
}
