using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;

namespace tanuki_proxy
{
    public class Utility
    {
        public static string Join(IEnumerable<string> command)
        {
            return string.Join(" ", command);
        }

        public static List<string> Split(string s)
        {
            return new List<string>(new Regex("\\s+").Split(s));
        }

        public static void WriteLineAndFlush(TextWriter textWriter, string format, params object[] arg)
        {
            textWriter.WriteLine(format, arg);
            textWriter.Flush();
        }
    }
}
