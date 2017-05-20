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
    class Program
    {
        public void Run(string[] args)
        {
            var options = new Options();
            try
            {
                options.Parse(args);
            }
            catch (ArgumentException e)
            {
                Environment.FailFast("Failed to start TanukiColiseum.", e);
            }

            var cli = new Cli();
            cli.Run(options);
        }

        static void Main(string[] args)
        {
            new Program().Run(args);
        }
    }
}
