using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.IO;

namespace tanuki_mate_engine_tester
{
    class Program
    {
        readonly string[] sfens = {
            "sfen l2g5/2s3g2/3k1p2p/P2pp2P1/1pP4s1/p1+B6/NP1P+nPS1P/K1G4+p1/L6NL b RBGNLPrs3p 1",
            "sfen 6lnk/6+Rbl/2n4pp/7s1/1p2P2NP/p1P2PPP1/1P4GS1/6GK1/LNr5L b B2G2S6Pp 1",
            "sfen lnks5/1pg1s4/2p5p/p4+r3/P1g6/1Nn6/BKN1P3P/9/LG2s4 w GSL2Prbl9p 1",
            "sfen l7l/2+Rbk4/3rp4/2p3pPs/p2P1p2p/2P1G4/P1N1PPN2/2GK2G2/L7L b B2S6Pgs2n 1",
            "sfen l5g1l/2s+B5/p2ppp2p/5kpP1/3n5/6Pp1/P3PP1lP/2+nr2SS1/3N1GKRL w G2Pbgsn3p 1",
            "sfen l4g2l/7k1/p1+Pp3pp/5ss1P/3Pp1gP1/P3SL3/N2GPK3/1+rP6/+p6RL w BG2N2Pbsn3p 1",
            "sfen l2s3nl/3g1p+R+R1/p1k5p/2pPp4/1p1p5/5Sp2/PPP1PP2P/3G5/L1K4NL b BG2S2Pbg2np 1",
            "sfen ln7/2gk1S+S2/2+rpPp2G/2p5p/PP4P2/3B4P/K1SP3PN/1Sg2P+np1/L+r6L w L2Pbgn3p 1",
            "sfen 6p1l/1+R1G2g2/5pns1/pp1pk3p/2p3P2/P7P/1L1PSP+b2/1SG1K2P1/L5G1L w N2Prbs2n3p 1",
            "sfen lng3+R2/2kgs4/ppp6/1B1pp4/7B1/2P2pLp1/PP1PP3P/1S1K2p2/LN5GL b RG2SP2n3p 1",
            };

        void Run(string[] args)
        {
            if (args.Length != 1)
            {
                Console.WriteLine("Usage: tanuki-mate-engine-tester.exe [mate engine path]");
                return;
            }

            var mateEnginePath = args[0];
            if (!File.Exists(mateEnginePath))
            {
                Console.WriteLine("Mate engine not found: mateEnginePath=" + mateEnginePath);
                return;
            }

            foreach (var sfen in sfens)
            {
                string checkmateLine = null;
                string time = null;
                using (var process = new Process())
                {
                    var processStartInfo = new ProcessStartInfo(mateEnginePath);
                    processStartInfo.RedirectStandardInput = true;
                    processStartInfo.RedirectStandardOutput = true;
                    processStartInfo.CreateNoWindow = false;
                    processStartInfo.UseShellExecute = false;
                    process.StartInfo = processStartInfo;
                    process.OutputDataReceived += new DataReceivedEventHandler(delegate (object sender, DataReceivedEventArgs e)
                    {
                        string line = e.Data;
                        if (line == null)
                        {
                            return;
                        }
                        else if (line.StartsWith("checkmate"))
                        {
                            checkmateLine = line;
                            process.StandardInput.WriteLine("quit");
                            process.StandardInput.Flush();
                        }
                        else if (line.Contains("time"))
                        {
                            var words = line.Split();
                            var list = words.ToList();
                            int timeIndex = list.IndexOf("time");
                            time = list[timeIndex + 1];
                        }
                    });
                    process.Start();
                    process.BeginOutputReadLine();


                    string[] input =
                    {
                        "usi",
                        "setoption name Hash value 1024",
                        "isready",
                        "usinewgame",
                        "position " + sfen,
                        "go mate 600000",
                    };

                    foreach (var line in input)
                    {
                        process.StandardInput.WriteLine(line);
                        process.StandardInput.Flush();
                    }

                    process.WaitForExit();
                }

                if (time == null)
                {
                    Console.WriteLine("-");
                }
                else
                {
                    Console.WriteLine(time);
                }
            }
        }

        static void Main(string[] args)
        {
            new Program().Run(args);
        }
    }
}
