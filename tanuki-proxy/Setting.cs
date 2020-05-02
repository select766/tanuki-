using System.Diagnostics;
using System.IO;
using System.Xml.Serialization;
using System.Runtime.Serialization;
using System.Collections.Generic;
using System.Linq;

namespace tanuki_proxy
{
    public class Setting
    {
        [DataContract]
        public class Option
        {
            [DataMember]
            public string name { get; set; }
            [DataMember]
            public string value { get; set; }
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
            public List<Option> optionOverrides { get; set; }
            [DataMember]
            public bool mateEngine { get; set; } = false;
        }

        [DataContract]
        public class ProxySetting
        {
            [DataMember]
            public List<EngineOption> engines { get; set; }
            [DataMember]
            public string logDirectory { get; set; }
        }

        public static void writeSampleSetting()
        {
            ProxySetting setting = new ProxySetting();
            setting.logDirectory = "C:\\home\\develop\\tanuki-";
            setting.engines = new List<EngineOption>
            {
                new EngineOption
                {
                    engineName="doutanuki",
                    fileName="C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                    arguments="",
                    workingDirectory="C:\\home\\develop\\tanuki-\\bin",
                    optionOverrides=new List<Option>
                    {
                        new Option{name="USI_Hash",value="1024",},
                        new Option{name="Book_File",value="../bin/book-2016-02-01.bin",},
                        new Option{name="Best_Book_Move",value="true",},
                        new Option{name="Max_Random_Score_Diff",value="0",},
                        new Option{name="Max_Random_Score_Diff_Ply",value="0",},
                        new Option{name="Threads",value="1",},
                    },
                    mateEngine=false,
                },
                new EngineOption
                {
                    engineName="nighthawk",
                    fileName="ssh",
                    arguments="-vvv nighthawk tanuki-.bat",
                    workingDirectory="C:\\home\\develop\\tanuki-\\bin",
                    optionOverrides=new List<Option>
                    {
                        new Option{name="USI_Hash", value="1024", },
                        new Option{name="Book_File", value="../bin/book-2016-02-01.bin", },
                        new Option{name="Best_Book_Move", value="true", },
                        new Option{name="Max_Random_Score_Diff", value="0", },
                        new Option{name="Max_Random_Score_Diff_Ply", value="0",},
                        new Option{name="Threads", value="4", },
                    },
                    mateEngine=false,
                },
                new EngineOption
                {
                    engineName="doutanuki",
                    fileName="C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                    arguments="",
                    workingDirectory="C:\\home\\develop\\tanuki-\\bin",
                    optionOverrides=new List<Option>
                    {
                        new Option{name="USI_Hash", value="1024" },
                        new Option{name="Book_File", value="../bin/book-2016-02-01.bin" },
                        new Option{name="Best_Book_Move", value="true" },
                        new Option{name="Max_Random_Score_Diff", value="0" },
                        new Option{name="Max_Random_Score_Diff_Ply", value="0" },
                        new Option{name="Threads", value="1" },
                    },
                    mateEngine=false,
                },
            };

            XmlSerializer serializer = new XmlSerializer(typeof(ProxySetting));
            using (FileStream f = new FileStream("proxy-setting.sample.xml", FileMode.Create))
            {
                serializer.Serialize(f, setting);
            }
        }

        public static ProxySetting loadSetting()
        {
            XmlSerializer serializer = new XmlSerializer(typeof(ProxySetting));
            // 1. まずはexeディレクトリに設定ファイルがあれば使う(複数Proxy設定をexeごとディレクトリに分け、カレントディレクトリは制御できない場合)
            // 2. それが無ければ、カレントディレクトリの設定を使う
            string[] search_dirs = { ExeDir(), "." };
            foreach (string search_dir in search_dirs)
            {
                string path = Path.Combine(search_dir, "proxy-setting.xml");
                if (File.Exists(path))
                {
                    using (FileStream f = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
                    {
                        return (ProxySetting)serializer.Deserialize(f);
                    }
                }
            }
            return null;
        }
        private static string ExeDir()
        {
            using (Process process = Process.GetCurrentProcess())
            {
                return Path.GetDirectoryName(process.MainModule.FileName);
            }
        }

        /// <summary>
        /// 設定ファイルを生成する。
        /// </summary>
        /// <param name="engineServerAddressFile">通常の思考エンジンのサーバーアドレスが1行に一つ書かれたテキストファイルへのパス</param>
        /// <param name="mateServerAddressFile">詰み探索専用思考エンジンのサーバーアドレスが1行に一つ書かれたテキストファイルへのパス</param>
        public static void CreateSettingFile(FileInfo engineServerAddressFile,
            FileInfo mateServerAddressFile)
        {
            var engineServerAddresses = File.ReadAllLines(engineServerAddressFile.FullName);
            var mateServerAddresses = File.ReadAllLines(mateServerAddressFile.FullName);

            var setting = new ProxySetting
            {
                logDirectory = "/home/ubuntu/log",
                engines = new List<EngineOption>(),
            };

            // 通常の思考エンジンの設定を追加する。
            foreach (var engineServerAddress in engineServerAddresses)
            {
                setting.engines.Add(new EngineOption
                {
                    engineName = "engineServerAddress",
                    fileName = "ssh",
                    arguments = $"ubuntu@{engineServerAddress} ./YaneuraOu-by-gcc-engine",
                    workingDirectory = "/home/ubuntu",
                    optionOverrides = new List<Option>
                    {
                        new Option{name="USI_Hash", value="65536" },
                        new Option{name="Threads", value="95" },
                        new Option{name="LazyClusterSendTo", value=engineServerAddresses
                            .Where(x=>x!=engineServerAddress)
                            .Select(x=>x+":30001")
                            .Aggregate((x,y)=>x+","+y)
                        },
                    },
                    mateEngine = false,
                });
            }

            // 詰み専用思考エンジンの設定を追加する。
            foreach (var mateServerAddress in mateServerAddresses)
            {
                setting.engines.Add(new EngineOption
                {
                    engineName = "engineServerAddress",
                    fileName = "ssh",
                    arguments = $"ubuntu@{mateServerAddress} ./YaneuraOu-by-gcc-mate",
                    workingDirectory = "/home/ubuntu",
                    optionOverrides = new List<Option>
                    {
                        new Option{name="USI_Hash", value="1024" },
                        new Option{name="Threads", value="1" },
                        new Option{name="LazyClusterSendTo", value=engineServerAddresses
                            .Select(x=>x+":30001")
                            .Aggregate((x,y)=>x+","+y)
                        },
                    },
                    mateEngine = true,
                });
            }

            var serializer = new XmlSerializer(typeof(ProxySetting));
            using (var f = new FileStream("proxy-setting.xml", FileMode.Create))
            {
                serializer.Serialize(f, setting);
            }
        }
    }
}
