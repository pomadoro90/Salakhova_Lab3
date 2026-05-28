using System;
using System.Windows.Forms;

namespace Salakhova_Sharp
{
    public partial class Form1 : Form
    {
        private SalakhovaSocketClient client = new SalakhovaSocketClient();
        private int myId = -1;
        private System.Windows.Forms.Timer pollTimer;
        private System.Windows.Forms.Timer pingTimer;

        public Form1()
        {
            InitializeComponent();
            this.FormClosing += Form1_FormClosing;
            this.Text = "Message Client";

            pollTimer = new System.Windows.Forms.Timer();
            pollTimer.Interval = 50;
            pollTimer.Tick += PollTimer_Tick;

            pingTimer = new System.Windows.Forms.Timer();
            pingTimer.Interval = 10000; // 10 секунд
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
            string host = string.IsNullOrWhiteSpace(txtHost.Text) ? "127.0.0.1" : txtHost.Text;
            int port = (int)numericPort.Value;

            try
            {
                if (!client.Connect(host, port))
                {
                    MessageBox.Show("Server rejected connection!", "Connection Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Connection error: {ex.Message}", "Connection Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            myId = client.ClientId;
            pollTimer.Start();
            pingTimer.Start();
            ToggleUi(true);
        }

        private void btnDisconnect_Click(object sender, EventArgs e)
        {
            DisconnectClient();
        }

        private void btnSend_Click(object sender, EventArgs e)
        {
            if (!client.IsConnected || string.IsNullOrWhiteSpace(textBoxMessage.Text)) return;
            if (comboRecipient.SelectedItem == null) return;

            int targetId = ((RecipientItem)comboRecipient.SelectedItem).Id;
            client.Send(targetId, SalakhovaSocketClient.MT_DATA, textBoxMessage.Text);
            txtOutput.AppendText($"[You -> {comboRecipient.SelectedItem}]: {textBoxMessage.Text}\r\n");
            textBoxMessage.Clear();
        }

        private void PingTimer_Tick(object sender, EventArgs e)
        {
            if (client.IsConnected)
            {
                client.Send(SalakhovaSocketClient.ADDR_SERVER, SalakhovaSocketClient.MT_INFO, "");
            }
        }

        private void PollTimer_Tick(object sender, EventArgs e)
        {
            int src, cmd, tgt;
            string text;

            while (client.Poll(out src, out cmd, out tgt, out text))
            {
                if (cmd == SalakhovaSocketClient.MT_CONFIRM)
                {
                    ParseClientList(text);
                }
                else if (cmd == SalakhovaSocketClient.MT_DATA)
                {
                    txtOutput.AppendText($"[From Client #{src}]: {text}\r\n");
                }
            }

            if (!client.IsConnected)
            {
                pollTimer.Stop();
                pingTimer.Stop();
                ToggleUi(false);
                MessageBox.Show("Connection to server lost!", "Disconnected",
                    MessageBoxButtons.OK, MessageBoxIcon.Warning);
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
            comboRecipient.Items.Add(new RecipientItem("All (Broadcast)", SalakhovaSocketClient.ADDR_BROADCAST));

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
            if (client.IsConnected)
            {
                client.Disconnect();
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
