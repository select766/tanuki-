using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Diagnostics;
using System.Text.RegularExpressions;

namespace tanuki_phoenix
{
    [DataContract]
    public class Settings
    {
        [DataMember]
        public string working_directory = "";
        [DataMember]
        public string exe_path = "";
        [DataMember]
        public List<string> arguments = new List<string>();
        [DataMember]
        public double retry_sleep_ms = 200;
        [DataMember]
        public int retry_count = 9999;
        // position/go関係はタイミングを間違えるとillegal moveにつながる。resend_goをfalseにすれば
        // position/goの再送を行わず、1指し手分の思考時間は無駄にすることで安全側に倒すことができる。
        [DataMember]
        public bool resend_go = false;
    }

    class Program : IDisposable
    {
        static string settings_file_name = "tanuki-phoenix.json";
        static string log_file_name = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "tanuki-phoenix.log");

        static Settings GetSettings()
        {
            string settings_file_path = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, settings_file_name);

            using (var fs = new System.IO.FileStream(settings_file_path, System.IO.FileMode.Open, System.IO.FileAccess.Read))
            {
                var serializer = new DataContractJsonSerializer(typeof(Settings));
                Settings s = (Settings)serializer.ReadObject(fs);

                return s;
            }
        }

        static void Main(string[] args)
        {
            using (Program p = new Program())
            {
                p.Start();
            }
        }

        Settings settings = GetSettings();

        // 上流から受け取ったusi setoptionのコマンドリスト
        List<string> m_usi_set_options = new List<string>();

        // 初回のUSIシーケンスやGO, BESTMOVEが完了する前にプロセスが死んだ場合、
        // 二個目以降のプロセスでもその直前まではエミュレートする必要がある。
        // また、GOを受けて思考中に死んだ場合は、再起動後にPOSITION、GOを再送する必要がある。
        enum EUSISequence
        {
            NONE,
            USI_USIOK,
            READY_READYOK,
            USINEWGAME,
            NO_POSITION_GO, // bestmoveにより古いposition/goが無効化された状態
            POSITION,
            GO,
        }
        EUSISequence m_initial_sequence_done = EUSISequence.NONE;
        string m_last_position = null;
        string m_last_go = null;

        // プロセス再起動回数
        int m_n_retried = 0;

        // ログ
        System.IO.StreamWriter m_log_writer = null;

        // 子プロセス
        Process m_active_process = null;

        // active_processの使用可能状態
        System.Threading.SemaphoreSlim m_process_semaphore = new System.Threading.SemaphoreSlim(0, 1);
        System.Threading.SemaphoreSlim m_process_stdout_semaphore = new System.Threading.SemaphoreSlim(0, 1);
        System.Threading.SemaphoreSlim m_process_stdin_semaphore = new System.Threading.SemaphoreSlim(0, 1);

        public void Log(string msg)
        {
            string timestr = DateTime.Now.ToLocalTime().ToString();
            m_log_writer.WriteLine(String.Format("{0}: {1}", timestr, msg));
        }

        public void Log(string format, params object[] objects)
        {
            Log(String.Format(format, objects));
        }

        public void Dispose()
        {
            m_log_writer?.Close();
        }
        
        void Start()
        {
            if (!System.IO.Directory.Exists(settings.working_directory))
            {
                Console.WriteLine("info ERROR working directory does not exist");
                return;
            }
            if (!System.IO.File.Exists(settings.exe_path))
            {
                Console.WriteLine("info ERROR exe does not exist");
                return;
            }

            m_log_writer = new System.IO.StreamWriter(System.IO.File.Open(log_file_name, System.IO.FileMode.Append, System.IO.FileAccess.Write));
            Log("log init");

            DoTask();
        }


        // 上流に送る
        void SendToUpstream(string line)
        {
            Console.WriteLine(line);
            Log("U< {0}", line);
        }

        // 上流からもらう
        async Task<string> RecvFromUpstreamAsync()
        {
            using (var stdin = Console.OpenStandardInput())
            using (var stdin_reader = new System.IO.StreamReader(stdin))
            {
                string line = await stdin_reader.ReadLineAsync();
                line = line.TrimEnd();
                Debug.WriteLine(String.Format("U> {0}", line));
                Log("U> {0}", line);
                return line;
            }
        }

        // 下流に送る
        async Task SendToDownstreamAsync(string line)
        {
            await m_process_stdin_semaphore.WaitAsync();
            await SendToDownstreamAsyncNoLock(line);
            m_process_stdin_semaphore.Release();
        }
        async Task SendToDownstreamAsyncNoLock(string line)
        {
            Debug.WriteLine(String.Format(">D {0}", line));
            Log(">D {0}", line);
            await m_active_process.StandardInput.WriteLineAsync(line);
        }

        // 下流からもらう
        async Task<string> RecvFromDownstreamAsync()
        {
            while (true)
            {
                await m_process_stdout_semaphore.WaitAsync();
                try
                {
                    string line = await m_active_process.StandardOutput.ReadLineAsync();
                    if (line == null)
                    {
                        // プロセスが死んだ場合
                        Debug.WriteLine("ERROR NULL read line");
                        continue;
                    }
                    line = line.TrimEnd();
                    Debug.WriteLine(String.Format("<D {0}", line));
                    Log("<D {0}", line);
                    return line;
                }
                catch
                {
                    Debug.WriteLine("CATCH");
                }
                finally
                {
                    m_process_stdout_semaphore.Release();
                }
            }
        }
        async Task<string> RecvFromDownstreamAsyncNoLock()
        {
            while (true)
            {
                string line = await m_active_process.StandardOutput.ReadLineAsync();
                if (line == null)
                {
                    Debug.WriteLine("ERROR NULL read line");
                    continue;
                }
                line = line.TrimEnd();
                Debug.WriteLine(String.Format("<D {0}", line));
                Log("<D {0}", line);
                return line;
            }
        }

        // 特定の文字列を待つ
        async Task WaitExpectedAsyncNoLock(string expect_line)
        {
            while (true)
            {
                Debug.WriteLine(String.Format("W) waiting downstream for [{0}] {1}", expect_line, m_active_process.HasExited));
                string line = await RecvFromDownstreamAsyncNoLock();
                if (line == expect_line)
                {
                    Debug.WriteLine(String.Format("W) match"));
                    return;
                }
                Debug.WriteLine(String.Format("W) not match"));
            }
        }


        void DoTask()
        {
            m_n_retried = 0;

            // 下流から受け取って上流に渡す
            Task downstream_to_upstream_task = Task.Run(async () =>
            {
                var name_pattern = new Regex("^id name (.*?)$");
                Debug.WriteLine("downstream_to_upstream_task started.");
                while (true)
                {
                    string line = await RecvFromDownstreamAsync();
                    
                    // 初期化シーケンスの完了をマーク
                    if (line == "usiok") m_initial_sequence_done = EUSISequence.USI_USIOK;
                    if (line == "readyok") m_initial_sequence_done = EUSISequence.READY_READYOK;
                    if (line.StartsWith("bestmove")) m_initial_sequence_done = EUSISequence.NO_POSITION_GO; // POSITION, GOを巻き戻す
                    
                    // プログラム名を書き換え
                    Match match = name_pattern.Match(line);
                    if (match.Success)
                    {
                        line = String.Format("id name {0}-phoenix", match.Groups[1].ToString());
                    }

                    // 上流に渡す
                    SendToUpstream(line);
                }
            });

            // 上流から受け取って下流に渡す
            Task upstream_to_downstream_task = Task.Run(async () =>
            {
                Debug.WriteLine("upstream_to_downstream_task started.");
                while (true)
                {
                    // 上流から読み込む
                    string line = await RecvFromUpstreamAsync();

                    // 初期化シーケンスの完了をマーク
                    if (line == "usinewgame") m_initial_sequence_done = EUSISequence.USINEWGAME;
                    // 状態の保存
                    if (line.StartsWith("position"))
                    {
                        m_last_position = line;
                        m_initial_sequence_done = EUSISequence.POSITION;
                    }
                    if (line.StartsWith("go"))
                    {
                        m_last_go = line;
                        m_initial_sequence_done = EUSISequence.GO;
                    }
                    // setoptionの保存
                    if (line.StartsWith("usi setoption"))
                    {
                        Debug.WriteLine("adding setoption.");
                        m_usi_set_options.Add(line);
                    }

                    // 下流に渡す
                    await SendToDownstreamAsync(line);

                    // quitは下流に伝達してからこのプロセスを終了
                    if (line == "quit")
                    {
                        Debug.WriteLine("quit..");
                        break;
                    }
                }

                Debug.WriteLine("upstream_to_downstream_task terminating.");
            });

            // プロセスを回す
            Task process_runner = Task.Run(async () =>
            {
                while (m_n_retried < settings.retry_count)
                {
                    await CreateProcessAndInitialize();
                    m_active_process.WaitForExit();
                    await Task.Delay((int)settings.retry_sleep_ms);
                }
            });

            try
            {
                upstream_to_downstream_task.Wait();
            }
            catch (AggregateException e)
            {
                Debug.WriteLine(String.Format("task terminated: {0}", e.Message));
            }
            Debug.WriteLine("M) finished");
        }

        // 子プロセスを開始
        async Task CreateProcessAndInitialize()
        {
            ProcessStartInfo info = new ProcessStartInfo(settings.exe_path, String.Join(" ", settings.arguments));
            info.CreateNoWindow = true;
            info.WorkingDirectory = settings.working_directory;
            info.RedirectStandardInput = true;
            info.RedirectStandardOutput = true;
            info.RedirectStandardError = false;
            info.ErrorDialog = false;
            info.UseShellExecute = false;
            
            Process p = Process.Start(info);

            p.EnableRaisingEvents = true;
            p.Exited += new EventHandler((object o, EventArgs e) =>
            {
                // 使用可能プロセスが減る
                Debug.WriteLine("process termination. lock the process...");
                m_process_semaphore.Wait();
                m_process_stdout_semaphore.Wait();
                m_process_stdin_semaphore.Wait();
                Debug.WriteLine("process termination. lock the process...OK");
            });

            Console.WriteLine("info string tanuki-phoenix: launching a process. {0}/{1}", m_n_retried, settings.retry_count);
            Debug.WriteLine(String.Format("launching #{0}/{1}-th process", m_n_retried, settings.retry_count));
            Log("LAUNCH");

            m_active_process = p;

            // 他のタスクが読み書きできるようになる前に初期化シーケンスを行う
            {
                Debug.WriteLine(String.Format("init sequence.."));
                if (m_initial_sequence_done >= EUSISequence.USI_USIOK)
                {
                    await SendToDownstreamAsyncNoLock("usi");
                    await WaitExpectedAsyncNoLock("usiok");
                }
                if (m_initial_sequence_done >= EUSISequence.READY_READYOK)
                {
                    foreach (string cmd in m_usi_set_options)
                    {
                        await SendToDownstreamAsyncNoLock(cmd);
                    }
                    await SendToDownstreamAsyncNoLock("isready");
                    await WaitExpectedAsyncNoLock("readyok");
                }
                if (m_initial_sequence_done >= EUSISequence.USINEWGAME)
                {
                    await SendToDownstreamAsyncNoLock("usinewgame");
                }
                if (settings.resend_go && m_initial_sequence_done >= EUSISequence.POSITION)
                {
                    await SendToDownstreamAsyncNoLock(m_last_position);
                }
                if (settings.resend_go && m_initial_sequence_done >= EUSISequence.GO)
                {
                    await SendToDownstreamAsyncNoLock(m_last_go);
                }
            }

            // 使用可能プロセスが増え、他のタスクで定常の読み書きを行う
            m_n_retried += 1;
            m_process_semaphore.Release();
            m_process_stdout_semaphore.Release();
            m_process_stdin_semaphore.Release();
            Debug.WriteLine("process init OK. release the process...OK");
        }

    }
}
