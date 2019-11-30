using System.Diagnostics;
using System.IO;
using System.Xml.Serialization;
using System.Runtime.Serialization;

namespace tanuki_proxy
{
    public class Setting
    {
        [DataContract]
        public class Option
        {
            [DataMember]
            public string name;
            [DataMember]
            public string value;

            public Option() { }

            public Option(string name, string value)
            {
                this.name = name;
                this.value = value;
            }
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
            public Option[] optionOverrides { get; set; }
            [DataMember]
            public bool mateEngine { get; set; } = false;

            public EngineOption(string engineName, string fileName, string arguments, string workingDirectory, Option[] optionOverrides, bool mateEngine)
            {
                this.engineName = engineName;
                this.fileName = fileName;
                this.arguments = arguments;
                this.workingDirectory = workingDirectory;
                this.optionOverrides = optionOverrides;
                this.mateEngine = mateEngine;
            }

            public EngineOption()
            {
                //empty constructor for serialization
            }
        }

        [DataContract]
        public class ProxySetting
        {
            [DataMember]
            public EngineOption[] engines { get; set; }
            [DataMember]
            public string logDirectory { get; set; }
        }

        public static void writeSampleSetting()
        {
            ProxySetting setting = new ProxySetting();
            setting.logDirectory = "C:\\home\\develop\\tanuki-";
            setting.engines = new EngineOption[]
            {
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                },false),
            new EngineOption(
                "nighthawk",
                "ssh",
                "-vvv nighthawk tanuki-.bat",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "4"),
                },false),
            new EngineOption(
                "doutanuki",
                "C:\\home\\develop\\tanuki-\\tanuki-\\x64\\Release\\tanuki-.exe",
                "",
                "C:\\home\\develop\\tanuki-\\bin",
                new[] {
                    new Option("USI_Hash", "1024"),
                    new Option("Book_File", "../bin/book-2016-02-01.bin"),
                    new Option("Best_Book_Move", "true"),
                    new Option("Max_Random_Score_Diff", "0"),
                    new Option("Max_Random_Score_Diff_Ply", "0"),
                    new Option("Threads", "1"),
                },false)
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
    }
}
