using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace Salakhova_Sharp
{
    public partial class Form1 : Form
    {
        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern bool Salakhova_Connect(string host, int port);

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void Salakhova_Disconnect();

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Salakhova_IsConnected();

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern void Salakhova_Send(int target, int command, string text);

        [DllImport("Salakhova_Transport.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern bool Salakhova_Poll(out int command, out int target, out int source, StringBuilder textBuf, int capacity);

        const int MT_CLOSE = 0;
        const int MT_DATA = 1;
        const int MT_QUIT = 4;
        const int MT_INFO = 5;
        const int MT_CONFIRM = 6;
        const int ADDR_BROADCAST = -1;
        const int ADDR_SERVER = -2;
        const int SERVER_TIMEOUT_SECONDS = 30;
        const int CLIENT_PING_INTERVAL_SEC = 10;

        private int myId = -1;
        private System.Windows.Forms.Timer pollTimer;
        private System.Windows.Forms.Timer pingTimer;
        private StringBuilder textBuf = new StringBuilder(4096);

        public Form1()
        {
            InitializeComponent();
            this.FormClosing += Form1_FormClosing;

            this.Text = "Message Client";

            pollTimer = new System.Windows.Forms.Timer();
            pollTimer.Interval = 50;
            pollTimer.Tick += PollTimer_Tick;

            pingTimer = new System.Windows.Forms.Timer();
            pingTimer.Interval = CLIENT_PING_INTERVAL_SEC * 1000;
            pingTimer.Tick += PingTimer_Tick;

            ToggleUi(false);
        }

        private void ToggleUi(bool isConnected)
        {
            btnConnect.Enabled = !isConnected;
            btnDisconnect.Enabled = isConnected;
            btnSend.Enabled = isConnected;
            comboRecipient.Enabled = isConnected;
            textBoxMessage.Enabled = isConnected;
        }

        private void btnConnect_Click(object sender, EventArgs e)
        {
            if (Salakhova_Connect("127.0.0.1", 12345))
            {
                pollTimer.Start();
                pingTimer.Start();
                ToggleUi(true);
            }
            else
            {
                MessageBox.Show("Server is not running!", "Connection Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            DisconnectClient();
        }

        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!Salakhova_IsConnected() || string.IsNullOrWhiteSpace(textBoxMessage.Text)) return;
            if (comboRecipient.SelectedItem == null) return;

            int targetId = ((RecipientItem)comboRecipient.SelectedItem).Id;
            Salakhova_Send(targetId, MT_DATA, textBoxMessage.Text);

            txtOutput.AppendText($"[You -> {comboRecipient.SelectedItem}]: {textBoxMessage.Text}\r\n");
            textBoxMessage.Clear();
        }

        private void PingTimer_Tick(object sender, EventArgs e)
        {
            if (Salakhova_IsConnected())
            {
                Salakhova_Send(ADDR_SERVER, MT_INFO, "");
            }
        }

        private void PollTimer_Tick(object sender, EventArgs e)
        {
            int cmd, tgt, src;
            while (Salakhova_Poll(out cmd, out tgt, out src, textBuf, textBuf.Capacity))
            {
                if (cmd == MT_CONFIRM)
                {
                    ParseClientList(textBuf.ToString());
                }
                else if (cmd == MT_DATA)
                {
                    txtOutput.AppendText($"[From Client #{src}]: {textBuf}\r\n");
                }
            }

            if (!Salakhova_IsConnected())
            {
                pollTimer.Stop();
                pingTimer.Stop();
                ToggleUi(false);
                MessageBox.Show("Connection to server lost!", "Disconnected", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }
        }

        private void ParseClientList(string payload)
        {
            // Если payload — это одно число без ':' (наш ID от сервера)
            if (!payload.Contains(":") && int.TryParse(payload, out int assignedId))
            {
                myId = assignedId;
                return;
            }

            int prevId = -999;
            if (comboRecipient.SelectedItem != null)
                prevId = ((RecipientItem)comboRecipient.SelectedItem).Id;

            comboRecipient.Items.Clear();
            comboRecipient.Items.Add(new RecipientItem("All (Broadcast)", ADDR_BROADCAST));

            if (!string.IsNullOrWhiteSpace(payload))
            {
                string[] clients = payload.Split(new[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
                foreach (var c in clients)
                {
                    string[] parts = c.Split(':');
                    if (parts.Length >= 2 && int.TryParse(parts[0], out int id))
                    {
                        string label = (id == myId) ? $"Client #{id} (You)" : $"Client #{id}";
                        comboRecipient.Items.Add(new RecipientItem(label, id));
                    }
                }
            }

            bool found = false;
            foreach (RecipientItem item in comboRecipient.Items)
            {
                if (item.Id == prevId) { comboRecipient.SelectedItem = item; found = true; break; }
            }
            if (!found && comboRecipient.Items.Count > 0) comboRecipient.SelectedIndex = 0;
        }

        private void DisconnectClient()
        {
            if (Salakhova_IsConnected())
            {
                Salakhova_Send(ADDR_SERVER, MT_QUIT, "");
                Salakhova_Disconnect();
                pollTimer.Stop();
                pingTimer.Stop();
                ToggleUi(false);
                comboRecipient.Items.Clear();
            }
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            DisconnectClient();
        }
    }

    public class RecipientItem
    {
        public string Name { get; }
        public int Id { get; }
        public RecipientItem(string name, int id) { Name = name; Id = id; }
        public override string ToString() => Name;
    }
}