using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace Salakhova_Sharp
{
    public partial class Form1 : Form
    {
        // ===== Импорт функций из DLL =====
        [DllImport("Salakhova_Transport.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool Salakhova_Connect(string ip, int port);

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern void Salakhova_Disconnect();

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern bool Salakhova_IsConnected();

        [DllImport("Salakhova_Transport.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern void Salakhova_Send(int target, int command, string text);

        [DllImport("Salakhova_Transport.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool Salakhova_Poll(out int outCommand, out int outTarget, StringBuilder outText, int outCapacity);

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int Salakhova_LastErrorCode();

        // ===== Константы протокола =====
        const int MT_CLOSE   = 0;
        const int MT_DATA    = 1;
        const int MT_START   = 2;
        const int MT_STOP    = 3;
        const int MT_QUIT    = 4;
        const int MT_INIT    = 5;
        const int MT_CONFIRM = 6;

        const int SR_ALL    = -1;
        const int SR_BROKER = -2;

        // ===== Таймер Poll =====
        private readonly Timer pollTimer = new Timer { Interval = 100 };

        public Form1()
        {
            InitializeComponent();
            UpdateUI(false);
            cbRecipient.Items.Add("Все клиенты");
            cbRecipient.SelectedIndex = 0;
        }

        // ===== Connect =====
        private void btnConnect_Click(object sender, EventArgs e)
        {
            string ip = string.IsNullOrWhiteSpace(txtIP.Text) ? "127.0.0.1" : txtIP.Text;
            int port = (int)numericPort.Value;

            try
            {
                if (!Salakhova_Connect(ip, port))
                {
                    int err = Salakhova_LastErrorCode();
                    MessageBox.Show($"Не удалось подключиться к {ip}:{port}\nКод ошибки: {err}",
                                    "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Ошибка подключения: {ex.Message}", "Ошибка", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            pollTimer.Start();
            UpdateUI(true);
        }

        // ===== Disconnect =====
        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            pollTimer.Stop();
            Salakhova_Disconnect();
            cbRecipient.Items.Clear();
            cbRecipient.Items.Add("Все клиенты");
            cbRecipient.SelectedIndex = 0;
            UpdateUI(false);
        }

        // ===== Start Thread =====
        private void btnStart_Click(object sender, EventArgs e)
        {
            if (!Salakhova_IsConnected()) return;
            Salakhova_Send(SR_BROKER, MT_START, "");
        }

        // ===== Stop Thread =====
        private void btnStop_Click(object sender, EventArgs e)
        {
            if (!Salakhova_IsConnected()) return;
            Salakhova_Send(SR_BROKER, MT_STOP, "");
        }

        // ===== Send Message =====
        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!Salakhova_IsConnected()) return;
            if (string.IsNullOrWhiteSpace(txtMessage.Text)) return;

            int target = cbRecipient.SelectedIndex <= 0 ? SR_ALL : threadIds[cbRecipient.SelectedIndex - 1];
            Salakhova_Send(target, MT_DATA, txtMessage.Text);
            txtMessage.Clear();
        }

        // ===== Poll Timer =====
        private void PollTimer_Tick(object sender, EventArgs e)
        {
            if (!Salakhova_IsConnected())
            {
                pollTimer.Stop();
                Salakhova_Disconnect();
                UpdateUI(false);
                MessageBox.Show("Соединение с сервером потеряно.", "Внимание", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            StringBuilder sb = new StringBuilder(1024);
            int cmd, target;
            while (Salakhova_Poll(out cmd, out target, sb, sb.Capacity))
            {
                string text = sb.ToString();
                switch (cmd)
                {
                    case MT_CONFIRM:
                        ParseThreadList(text);
                        break;

                    case MT_DATA:
                        txtOutput.AppendText($"[{DateTime.Now:HH:mm:ss}] {text}\r\n");
                        break;
                }
            }
        }

        // ===== Парсинг списка потоков из MT_CONFIRM =====
        private int[] threadIds = new int[0];

        private void ParseThreadList(string text)
        {
            if (string.IsNullOrWhiteSpace(text))
            {
                threadIds = new int[0];
            }
            else
            {
                string[] parts = text.Split(',');
                threadIds = new int[parts.Length];
                for (int i = 0; i < parts.Length; i++)
                    int.TryParse(parts[i], out threadIds[i]);
            }

            cbRecipient.Items.Clear();
            cbRecipient.Items.Add("Все клиенты");
            for (int i = 0; i < threadIds.Length; i++)
                cbRecipient.Items.Add($"Поток {threadIds[i]}");
            cbRecipient.SelectedIndex = 0;
        }

        // ===== Обновление UI =====
        private void UpdateUI(bool connected)
        {
            btnConnect.Enabled    = !connected;
            btnDisconnect.Enabled = connected;
            txtIP.Enabled         = !connected;
            numericPort.Enabled   = !connected;
            btnStart.Enabled      = connected;
            btnStop.Enabled       = connected;
            btnSend.Enabled       = connected;
            txtMessage.Enabled    = connected;
            cbRecipient.Enabled   = connected;

            Text = connected
                ? $"Салахова Lab3 | подключено к {txtIP.Text}:{numericPort.Value}"
                : "Салахова Lab3 | не подключено";
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            pollTimer.Stop();
            Salakhova_Disconnect();
        }
    }
}