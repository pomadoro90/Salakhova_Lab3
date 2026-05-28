namespace Salakhova_Sharp
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.btnConnect = new System.Windows.Forms.Button();
            this.btnDisconnect = new System.Windows.Forms.Button();
            this.txtOutput = new System.Windows.Forms.TextBox();
            this.comboRecipient = new System.Windows.Forms.ComboBox();
            this.textBoxMessage = new System.Windows.Forms.TextBox();
            this.btnSend = new System.Windows.Forms.Button();
            this.txtHost = new System.Windows.Forms.TextBox();
            this.numericPort = new System.Windows.Forms.NumericUpDown();
            ((System.ComponentModel.ISupportInitialize)(this.numericPort)).BeginInit();
            this.SuspendLayout();
            // 
            // btnConnect
            // 
            this.btnConnect.Location = new System.Drawing.Point(30, 55);
            this.btnConnect.Name = "btnConnect";
            this.btnConnect.Size = new System.Drawing.Size(200, 45);
            this.btnConnect.TabIndex = 2;
            this.btnConnect.Text = "Connect";
            this.btnConnect.UseVisualStyleBackColor = true;
            this.btnConnect.Click += new System.EventHandler(this.btnConnect_Click);
            // 
            // btnDisconnect
            // 
            this.btnDisconnect.Location = new System.Drawing.Point(250, 55);
            this.btnDisconnect.Name = "btnDisconnect";
            this.btnDisconnect.Size = new System.Drawing.Size(200, 45);
            this.btnDisconnect.TabIndex = 3;
            this.btnDisconnect.Text = "Disconnect";
            this.btnDisconnect.UseVisualStyleBackColor = true;
            this.btnDisconnect.Click += new System.EventHandler(this.btnDisconnect_Click);
            // 
            // txtOutput
            // 
            this.txtOutput.Location = new System.Drawing.Point(30, 115);
            this.txtOutput.Multiline = true;
            this.txtOutput.Name = "txtOutput";
            this.txtOutput.ReadOnly = true;
            this.txtOutput.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.txtOutput.Size = new System.Drawing.Size(600, 370);
            this.txtOutput.TabIndex = 4;
            // 
            // comboRecipient
            // 
            this.comboRecipient.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.comboRecipient.FormattingEnabled = true;
            this.comboRecipient.Location = new System.Drawing.Point(30, 500);
            this.comboRecipient.Name = "comboRecipient";
            this.comboRecipient.Size = new System.Drawing.Size(250, 40);
            this.comboRecipient.TabIndex = 5;
            // 
            // textBoxMessage
            // 
            this.textBoxMessage.Location = new System.Drawing.Point(300, 500);
            this.textBoxMessage.Name = "textBoxMessage";
            this.textBoxMessage.Size = new System.Drawing.Size(330, 39);
            this.textBoxMessage.TabIndex = 6;
            // 
            // btnSend
            // 
            this.btnSend.Location = new System.Drawing.Point(30, 555);
            this.btnSend.Name = "btnSend";
            this.btnSend.Size = new System.Drawing.Size(600, 50);
            this.btnSend.TabIndex = 7;
            this.btnSend.Text = "Send Message";
            this.btnSend.UseVisualStyleBackColor = true;
            this.btnSend.Click += new System.EventHandler(this.btnSend_Click);
            // 
            // txtHost
            // 
            this.txtHost.Location = new System.Drawing.Point(30, 15);
            this.txtHost.Name = "txtHost";
            this.txtHost.Size = new System.Drawing.Size(350, 39);
            this.txtHost.TabIndex = 0;
            this.txtHost.Text = "127.0.0.1";
            // 
            // numericPort
            // 
            this.numericPort.Location = new System.Drawing.Point(400, 15);
            this.numericPort.Maximum = new decimal(new int[] { 65535, 0, 0, 0 });
            this.numericPort.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
            this.numericPort.Name = "numericPort";
            this.numericPort.Size = new System.Drawing.Size(100, 39);
            this.numericPort.TabIndex = 1;
            this.numericPort.Value = new decimal(new int[] { 12345, 0, 0, 0 });
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(13F, 32F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(665, 620);
            this.Controls.Add(this.numericPort);
            this.Controls.Add(this.txtHost);
            this.Controls.Add(this.btnSend);
            this.Controls.Add(this.textBoxMessage);
            this.Controls.Add(this.comboRecipient);
            this.Controls.Add(this.txtOutput);
            this.Controls.Add(this.btnDisconnect);
            this.Controls.Add(this.btnConnect);
            this.Margin = new System.Windows.Forms.Padding(5);
            this.Name = "Form1";
            this.Text = "Message Client";
            ((System.ComponentModel.ISupportInitialize)(this.numericPort)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button btnConnect;
        private System.Windows.Forms.Button btnDisconnect;
        private System.Windows.Forms.TextBox txtOutput;
        private System.Windows.Forms.ComboBox comboRecipient;
        private System.Windows.Forms.TextBox textBoxMessage;
        private System.Windows.Forms.Button btnSend;
        private System.Windows.Forms.TextBox txtHost;
        private System.Windows.Forms.NumericUpDown numericPort;
    }
}
