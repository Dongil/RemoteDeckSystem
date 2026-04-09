namespace RemoteDeck
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
			groupBox1 = new GroupBox();
			groupBox3 = new GroupBox();
			btnGetAllStatus = new Button();
			btnClearLog = new Button();
			richLogger = new RichTextBox();
			btnMqttDiscon = new Button();
			btnMqttConn = new Button();
			groupBox2 = new GroupBox();
			label4 = new Label();
			txtSubcribe = new TextBox();
			label3 = new Label();
			txtPublish = new TextBox();
			btnConfigSave = new Button();
			label2 = new Label();
			txtPort = new TextBox();
			label1 = new Label();
			txtIp = new TextBox();
			groupBox4 = new GroupBox();
			groupBox6 = new GroupBox();
			btnRelay2OFF = new Button();
			btnRelay2ON = new Button();
			groupBox5 = new GroupBox();
			btnRelay1OFF = new Button();
			btnRelay1ON = new Button();
			groupBox7 = new GroupBox();
			btnIO3ON = new Button();
			label13 = new Label();
			btnIO7OFF = new Button();
			btnIO7ON = new Button();
			label8 = new Label();
			btnIO6OFF = new Button();
			btnIO6ON = new Button();
			label9 = new Label();
			btnIO5OFF = new Button();
			btnIO5ON = new Button();
			label10 = new Label();
			btnIO4OFF = new Button();
			btnIO4ON = new Button();
			label7 = new Label();
			btnIO3OFF = new Button();
			label6 = new Label();
			btnIO2OFF = new Button();
			btnIO2ON = new Button();
			label5 = new Label();
			btnIO1OFF = new Button();
			btnIO1ON = new Button();
			groupBox8 = new GroupBox();
			btnGetLed = new Button();
			groupBox9 = new GroupBox();
			btnLedON = new Button();
			btnLedOFF = new Button();
			groupBox10 = new GroupBox();
			btnIrON = new Button();
			btnIrOFF = new Button();
			groupBox11 = new GroupBox();
			btnHdmiON = new Button();
			btnHdmiOFF = new Button();
			groupBox12 = new GroupBox();
			textBox2 = new TextBox();
			label12 = new Label();
			label11 = new Label();
			textBox1 = new TextBox();
			groupBox13 = new GroupBox();
			btn485Relay1OFF = new Button();
			btn485Relay1ON = new Button();
			btn485Conn = new Button();
			btn485Disconn = new Button();
			textBox5 = new TextBox();
			groupBox1.SuspendLayout();
			groupBox3.SuspendLayout();
			groupBox2.SuspendLayout();
			groupBox4.SuspendLayout();
			groupBox6.SuspendLayout();
			groupBox5.SuspendLayout();
			groupBox7.SuspendLayout();
			groupBox8.SuspendLayout();
			groupBox9.SuspendLayout();
			groupBox10.SuspendLayout();
			groupBox11.SuspendLayout();
			groupBox12.SuspendLayout();
			groupBox13.SuspendLayout();
			SuspendLayout();
			// 
			// groupBox1
			// 
			groupBox1.Controls.Add(groupBox3);
			groupBox1.Controls.Add(btnMqttDiscon);
			groupBox1.Controls.Add(btnMqttConn);
			groupBox1.Controls.Add(groupBox2);
			groupBox1.Controls.Add(btnConfigSave);
			groupBox1.Controls.Add(label2);
			groupBox1.Controls.Add(txtPort);
			groupBox1.Controls.Add(label1);
			groupBox1.Controls.Add(txtIp);
			groupBox1.Location = new Point(12, 22);
			groupBox1.Name = "groupBox1";
			groupBox1.Size = new Size(369, 586);
			groupBox1.TabIndex = 0;
			groupBox1.TabStop = false;
			groupBox1.Text = "Mqtt 서버 연결";
			// 
			// groupBox3
			// 
			groupBox3.Controls.Add(btnGetAllStatus);
			groupBox3.Controls.Add(btnClearLog);
			groupBox3.Controls.Add(richLogger);
			groupBox3.Location = new Point(24, 260);
			groupBox3.Name = "groupBox3";
			groupBox3.Size = new Size(305, 309);
			groupBox3.TabIndex = 10;
			groupBox3.TabStop = false;
			// 
			// btnGetAllStatus
			// 
			btnGetAllStatus.Location = new Point(10, 21);
			btnGetAllStatus.Name = "btnGetAllStatus";
			btnGetAllStatus.Size = new Size(132, 23);
			btnGetAllStatus.TabIndex = 12;
			btnGetAllStatus.Text = "전체 상태 가져오기";
			btnGetAllStatus.UseVisualStyleBackColor = true;
			btnGetAllStatus.Click += btnGetAllStatus_Click;
			// 
			// btnClearLog
			// 
			btnClearLog.Location = new Point(220, 21);
			btnClearLog.Name = "btnClearLog";
			btnClearLog.Size = new Size(75, 23);
			btnClearLog.TabIndex = 11;
			btnClearLog.Text = "내용 삭제";
			btnClearLog.UseVisualStyleBackColor = true;
			btnClearLog.Click += btnClearLog_Click;
			// 
			// richLogger
			// 
			richLogger.Location = new Point(10, 54);
			richLogger.Name = "richLogger";
			richLogger.Size = new Size(285, 247);
			richLogger.TabIndex = 0;
			richLogger.Text = "";
			// 
			// btnMqttDiscon
			// 
			btnMqttDiscon.Location = new Point(176, 215);
			btnMqttDiscon.Name = "btnMqttDiscon";
			btnMqttDiscon.Size = new Size(75, 23);
			btnMqttDiscon.TabIndex = 9;
			btnMqttDiscon.Text = "정지";
			btnMqttDiscon.UseVisualStyleBackColor = true;
			btnMqttDiscon.Click += btnMqttDiscon_Click;
			// 
			// btnMqttConn
			// 
			btnMqttConn.Location = new Point(80, 215);
			btnMqttConn.Name = "btnMqttConn";
			btnMqttConn.Size = new Size(75, 23);
			btnMqttConn.TabIndex = 8;
			btnMqttConn.Text = "연결";
			btnMqttConn.UseVisualStyleBackColor = true;
			btnMqttConn.Click += btnMqttConn_Click;
			// 
			// groupBox2
			// 
			groupBox2.Controls.Add(label4);
			groupBox2.Controls.Add(txtSubcribe);
			groupBox2.Controls.Add(label3);
			groupBox2.Controls.Add(txtPublish);
			groupBox2.Location = new Point(24, 96);
			groupBox2.Name = "groupBox2";
			groupBox2.Size = new Size(305, 89);
			groupBox2.TabIndex = 7;
			groupBox2.TabStop = false;
			// 
			// label4
			// 
			label4.AutoSize = true;
			label4.Location = new Point(16, 25);
			label4.Name = "label4";
			label4.Size = new Size(99, 15);
			label4.TabIndex = 8;
			label4.Text = "Subscribe Topic :";
			// 
			// txtSubcribe
			// 
			txtSubcribe.Location = new Point(124, 21);
			txtSubcribe.Name = "txtSubcribe";
			txtSubcribe.Size = new Size(132, 23);
			txtSubcribe.TabIndex = 7;
			txtSubcribe.Text = "RemoteDeck/client";
			// 
			// label3
			// 
			label3.AutoSize = true;
			label3.Location = new Point(29, 60);
			label3.Name = "label3";
			label3.Size = new Size(86, 15);
			label3.TabIndex = 6;
			label3.Text = "Publish Topic :";
			// 
			// txtPublish
			// 
			txtPublish.Location = new Point(126, 56);
			txtPublish.Name = "txtPublish";
			txtPublish.Size = new Size(132, 23);
			txtPublish.TabIndex = 5;
			txtPublish.Text = "RemoteDeck/node_1";
			// 
			// btnConfigSave
			// 
			btnConfigSave.Location = new Point(254, 34);
			btnConfigSave.Name = "btnConfigSave";
			btnConfigSave.Size = new Size(75, 23);
			btnConfigSave.TabIndex = 4;
			btnConfigSave.Text = "저장";
			btnConfigSave.UseVisualStyleBackColor = true;
			btnConfigSave.Click += btnConfigSave_Click;
			// 
			// label2
			// 
			label2.AutoSize = true;
			label2.Location = new Point(24, 71);
			label2.Name = "label2";
			label2.Size = new Size(36, 15);
			label2.TabIndex = 3;
			label2.Text = "Port :";
			// 
			// txtPort
			// 
			txtPort.Location = new Point(80, 67);
			txtPort.Name = "txtPort";
			txtPort.Size = new Size(64, 23);
			txtPort.TabIndex = 2;
			txtPort.Text = "1883";
			// 
			// label1
			// 
			label1.AutoSize = true;
			label1.Location = new Point(24, 38);
			label1.Name = "label1";
			label1.Size = new Size(50, 15);
			label1.TabIndex = 1;
			label1.Text = "IP/URL :";
			// 
			// txtIp
			// 
			txtIp.Location = new Point(80, 34);
			txtIp.Name = "txtIp";
			txtIp.Size = new Size(145, 23);
			txtIp.TabIndex = 0;
			// 
			// groupBox4
			// 
			groupBox4.Controls.Add(groupBox6);
			groupBox4.Controls.Add(groupBox5);
			groupBox4.Location = new Point(387, 22);
			groupBox4.Name = "groupBox4";
			groupBox4.Size = new Size(259, 200);
			groupBox4.TabIndex = 1;
			groupBox4.TabStop = false;
			groupBox4.Text = "릴레이 테스트";
			// 
			// groupBox6
			// 
			groupBox6.Controls.Add(btnRelay2OFF);
			groupBox6.Controls.Add(btnRelay2ON);
			groupBox6.Location = new Point(133, 22);
			groupBox6.Name = "groupBox6";
			groupBox6.Size = new Size(117, 74);
			groupBox6.TabIndex = 13;
			groupBox6.TabStop = false;
			groupBox6.Text = "Relay2";
			// 
			// btnRelay2OFF
			// 
			btnRelay2OFF.Location = new Point(63, 31);
			btnRelay2OFF.Name = "btnRelay2OFF";
			btnRelay2OFF.Size = new Size(40, 22);
			btnRelay2OFF.TabIndex = 12;
			btnRelay2OFF.Text = "OFF";
			btnRelay2OFF.UseVisualStyleBackColor = true;
			btnRelay2OFF.Click += btnRelayOFF_Click;
			// 
			// btnRelay2ON
			// 
			btnRelay2ON.Location = new Point(19, 31);
			btnRelay2ON.Name = "btnRelay2ON";
			btnRelay2ON.Size = new Size(40, 22);
			btnRelay2ON.TabIndex = 11;
			btnRelay2ON.Text = "ON";
			btnRelay2ON.UseVisualStyleBackColor = true;
			btnRelay2ON.Click += btnRelayON_Click;
			// 
			// groupBox5
			// 
			groupBox5.Controls.Add(btnRelay1OFF);
			groupBox5.Controls.Add(btnRelay1ON);
			groupBox5.Location = new Point(10, 22);
			groupBox5.Name = "groupBox5";
			groupBox5.Size = new Size(117, 74);
			groupBox5.TabIndex = 10;
			groupBox5.TabStop = false;
			groupBox5.Text = "Relay1";
			// 
			// btnRelay1OFF
			// 
			btnRelay1OFF.Location = new Point(63, 31);
			btnRelay1OFF.Name = "btnRelay1OFF";
			btnRelay1OFF.Size = new Size(40, 22);
			btnRelay1OFF.TabIndex = 12;
			btnRelay1OFF.Text = "OFF";
			btnRelay1OFF.UseVisualStyleBackColor = true;
			btnRelay1OFF.Click += btnRelayOFF_Click;
			// 
			// btnRelay1ON
			// 
			btnRelay1ON.Location = new Point(19, 31);
			btnRelay1ON.Name = "btnRelay1ON";
			btnRelay1ON.Size = new Size(40, 22);
			btnRelay1ON.TabIndex = 11;
			btnRelay1ON.Text = "ON";
			btnRelay1ON.UseVisualStyleBackColor = true;
			btnRelay1ON.Click += btnRelayON_Click;
			// 
			// groupBox7
			// 
			groupBox7.Controls.Add(btnIO3ON);
			groupBox7.Controls.Add(label13);
			groupBox7.Controls.Add(btnIO7OFF);
			groupBox7.Controls.Add(btnIO7ON);
			groupBox7.Controls.Add(label8);
			groupBox7.Controls.Add(btnIO6OFF);
			groupBox7.Controls.Add(btnIO6ON);
			groupBox7.Controls.Add(label9);
			groupBox7.Controls.Add(btnIO5OFF);
			groupBox7.Controls.Add(btnIO5ON);
			groupBox7.Controls.Add(label10);
			groupBox7.Controls.Add(btnIO4OFF);
			groupBox7.Controls.Add(btnIO4ON);
			groupBox7.Controls.Add(label7);
			groupBox7.Controls.Add(btnIO3OFF);
			groupBox7.Controls.Add(label6);
			groupBox7.Controls.Add(btnIO2OFF);
			groupBox7.Controls.Add(btnIO2ON);
			groupBox7.Controls.Add(label5);
			groupBox7.Controls.Add(btnIO1OFF);
			groupBox7.Controls.Add(btnIO1ON);
			groupBox7.Location = new Point(652, 22);
			groupBox7.Name = "groupBox7";
			groupBox7.Size = new Size(349, 200);
			groupBox7.TabIndex = 2;
			groupBox7.TabStop = false;
			groupBox7.Text = "I/O";
			// 
			// btnIO3ON
			// 
			btnIO3ON.Location = new Point(109, 44);
			btnIO3ON.Name = "btnIO3ON";
			btnIO3ON.Size = new Size(40, 22);
			btnIO3ON.TabIndex = 32;
			btnIO3ON.Text = "ON";
			btnIO3ON.UseVisualStyleBackColor = true;
			btnIO3ON.Click += btnIOON_Click;
			// 
			// label13
			// 
			label13.AutoSize = true;
			label13.Location = new Point(307, 22);
			label13.Name = "label13";
			label13.Size = new Size(14, 15);
			label13.TabIndex = 31;
			label13.Text = "7";
			// 
			// btnIO7OFF
			// 
			btnIO7OFF.Location = new Point(295, 70);
			btnIO7OFF.Name = "btnIO7OFF";
			btnIO7OFF.Size = new Size(40, 22);
			btnIO7OFF.TabIndex = 30;
			btnIO7OFF.Text = "OFF";
			btnIO7OFF.UseVisualStyleBackColor = true;
			btnIO7OFF.Click += btnIOOFF_Click;
			// 
			// btnIO7ON
			// 
			btnIO7ON.Location = new Point(295, 44);
			btnIO7ON.Name = "btnIO7ON";
			btnIO7ON.Size = new Size(40, 22);
			btnIO7ON.TabIndex = 29;
			btnIO7ON.Text = "ON";
			btnIO7ON.UseVisualStyleBackColor = true;
			btnIO7ON.Click += btnIOON_Click;
			// 
			// label8
			// 
			label8.AutoSize = true;
			label8.Location = new Point(260, 22);
			label8.Name = "label8";
			label8.Size = new Size(14, 15);
			label8.TabIndex = 28;
			label8.Text = "6";
			// 
			// btnIO6OFF
			// 
			btnIO6OFF.Location = new Point(248, 70);
			btnIO6OFF.Name = "btnIO6OFF";
			btnIO6OFF.Size = new Size(40, 22);
			btnIO6OFF.TabIndex = 27;
			btnIO6OFF.Text = "OFF";
			btnIO6OFF.UseVisualStyleBackColor = true;
			btnIO6OFF.Click += btnIOOFF_Click;
			// 
			// btnIO6ON
			// 
			btnIO6ON.Location = new Point(248, 44);
			btnIO6ON.Name = "btnIO6ON";
			btnIO6ON.Size = new Size(40, 22);
			btnIO6ON.TabIndex = 26;
			btnIO6ON.Text = "ON";
			btnIO6ON.UseVisualStyleBackColor = true;
			btnIO6ON.Click += btnIOON_Click;
			// 
			// label9
			// 
			label9.AutoSize = true;
			label9.Location = new Point(214, 22);
			label9.Name = "label9";
			label9.Size = new Size(14, 15);
			label9.TabIndex = 25;
			label9.Text = "5";
			// 
			// btnIO5OFF
			// 
			btnIO5OFF.Location = new Point(202, 70);
			btnIO5OFF.Name = "btnIO5OFF";
			btnIO5OFF.Size = new Size(40, 22);
			btnIO5OFF.TabIndex = 24;
			btnIO5OFF.Text = "OFF";
			btnIO5OFF.UseVisualStyleBackColor = true;
			btnIO5OFF.Click += btnIOOFF_Click;
			// 
			// btnIO5ON
			// 
			btnIO5ON.Location = new Point(202, 44);
			btnIO5ON.Name = "btnIO5ON";
			btnIO5ON.Size = new Size(40, 22);
			btnIO5ON.TabIndex = 23;
			btnIO5ON.Text = "ON";
			btnIO5ON.UseVisualStyleBackColor = true;
			btnIO5ON.Click += btnIOON_Click;
			// 
			// label10
			// 
			label10.AutoSize = true;
			label10.Location = new Point(168, 22);
			label10.Name = "label10";
			label10.Size = new Size(14, 15);
			label10.TabIndex = 22;
			label10.Text = "4";
			// 
			// btnIO4OFF
			// 
			btnIO4OFF.Location = new Point(156, 70);
			btnIO4OFF.Name = "btnIO4OFF";
			btnIO4OFF.Size = new Size(40, 22);
			btnIO4OFF.TabIndex = 21;
			btnIO4OFF.Text = "OFF";
			btnIO4OFF.UseVisualStyleBackColor = true;
			btnIO4OFF.Click += btnIOOFF_Click;
			// 
			// btnIO4ON
			// 
			btnIO4ON.Location = new Point(156, 44);
			btnIO4ON.Name = "btnIO4ON";
			btnIO4ON.Size = new Size(40, 22);
			btnIO4ON.TabIndex = 20;
			btnIO4ON.Text = "ON";
			btnIO4ON.UseVisualStyleBackColor = true;
			btnIO4ON.Click += btnIOON_Click;
			// 
			// label7
			// 
			label7.AutoSize = true;
			label7.Location = new Point(121, 22);
			label7.Name = "label7";
			label7.Size = new Size(14, 15);
			label7.TabIndex = 19;
			label7.Text = "3";
			// 
			// btnIO3OFF
			// 
			btnIO3OFF.Location = new Point(109, 70);
			btnIO3OFF.Name = "btnIO3OFF";
			btnIO3OFF.Size = new Size(40, 22);
			btnIO3OFF.TabIndex = 18;
			btnIO3OFF.Text = "OFF";
			btnIO3OFF.UseVisualStyleBackColor = true;
			btnIO3OFF.Click += btnIOOFF_Click;
			// 
			// label6
			// 
			label6.AutoSize = true;
			label6.Location = new Point(75, 22);
			label6.Name = "label6";
			label6.Size = new Size(14, 15);
			label6.TabIndex = 16;
			label6.Text = "2";
			// 
			// btnIO2OFF
			// 
			btnIO2OFF.Location = new Point(63, 70);
			btnIO2OFF.Name = "btnIO2OFF";
			btnIO2OFF.Size = new Size(40, 22);
			btnIO2OFF.TabIndex = 15;
			btnIO2OFF.Text = "OFF";
			btnIO2OFF.UseVisualStyleBackColor = true;
			btnIO2OFF.Click += btnIOOFF_Click;
			// 
			// btnIO2ON
			// 
			btnIO2ON.Location = new Point(63, 44);
			btnIO2ON.Name = "btnIO2ON";
			btnIO2ON.Size = new Size(40, 22);
			btnIO2ON.TabIndex = 14;
			btnIO2ON.Text = "ON";
			btnIO2ON.UseVisualStyleBackColor = true;
			btnIO2ON.Click += btnIOON_Click;
			// 
			// label5
			// 
			label5.AutoSize = true;
			label5.Location = new Point(29, 22);
			label5.Name = "label5";
			label5.Size = new Size(14, 15);
			label5.TabIndex = 13;
			label5.Text = "1";
			// 
			// btnIO1OFF
			// 
			btnIO1OFF.Location = new Point(17, 70);
			btnIO1OFF.Name = "btnIO1OFF";
			btnIO1OFF.Size = new Size(40, 22);
			btnIO1OFF.TabIndex = 12;
			btnIO1OFF.Text = "OFF";
			btnIO1OFF.UseVisualStyleBackColor = true;
			btnIO1OFF.Click += btnIOOFF_Click;
			// 
			// btnIO1ON
			// 
			btnIO1ON.Location = new Point(17, 44);
			btnIO1ON.Name = "btnIO1ON";
			btnIO1ON.Size = new Size(40, 22);
			btnIO1ON.TabIndex = 11;
			btnIO1ON.Text = "ON";
			btnIO1ON.UseVisualStyleBackColor = true;
			btnIO1ON.Click += btnIOON_Click;
			// 
			// groupBox8
			// 
			groupBox8.Controls.Add(btnGetLed);
			groupBox8.Controls.Add(groupBox9);
			groupBox8.Location = new Point(1007, 22);
			groupBox8.Name = "groupBox8";
			groupBox8.Size = new Size(73, 200);
			groupBox8.TabIndex = 3;
			groupBox8.TabStop = false;
			groupBox8.Text = "PC LED";
			// 
			// btnGetLed
			// 
			btnGetLed.Location = new Point(9, 44);
			btnGetLed.Name = "btnGetLed";
			btnGetLed.Size = new Size(55, 48);
			btnGetLed.TabIndex = 31;
			btnGetLed.Text = "Get Status";
			btnGetLed.UseVisualStyleBackColor = true;
			btnGetLed.Click += btnGetLed_Click;
			// 
			// groupBox9
			// 
			groupBox9.Controls.Add(btnLedON);
			groupBox9.Controls.Add(btnLedOFF);
			groupBox9.Location = new Point(6, 103);
			groupBox9.Name = "groupBox9";
			groupBox9.Size = new Size(61, 84);
			groupBox9.TabIndex = 14;
			groupBox9.TabStop = false;
			groupBox9.Text = "Status";
			// 
			// btnLedON
			// 
			btnLedON.Location = new Point(10, 23);
			btnLedON.Name = "btnLedON";
			btnLedON.Size = new Size(40, 22);
			btnLedON.TabIndex = 11;
			btnLedON.Text = "ON";
			btnLedON.UseVisualStyleBackColor = true;
			// 
			// btnLedOFF
			// 
			btnLedOFF.Location = new Point(10, 51);
			btnLedOFF.Name = "btnLedOFF";
			btnLedOFF.Size = new Size(40, 22);
			btnLedOFF.TabIndex = 12;
			btnLedOFF.Text = "OFF";
			btnLedOFF.UseVisualStyleBackColor = true;
			// 
			// groupBox10
			// 
			groupBox10.Controls.Add(btnIrON);
			groupBox10.Controls.Add(btnIrOFF);
			groupBox10.Location = new Point(1086, 21);
			groupBox10.Name = "groupBox10";
			groupBox10.Size = new Size(73, 200);
			groupBox10.TabIndex = 4;
			groupBox10.TabStop = false;
			groupBox10.Text = "IR";
			// 
			// btnIrON
			// 
			btnIrON.Location = new Point(15, 44);
			btnIrON.Name = "btnIrON";
			btnIrON.Size = new Size(40, 22);
			btnIrON.TabIndex = 11;
			btnIrON.Text = "ON";
			btnIrON.UseVisualStyleBackColor = true;
			btnIrON.Click += btnIrON_Click;
			// 
			// btnIrOFF
			// 
			btnIrOFF.Location = new Point(15, 72);
			btnIrOFF.Name = "btnIrOFF";
			btnIrOFF.Size = new Size(40, 22);
			btnIrOFF.TabIndex = 12;
			btnIrOFF.Text = "OFF";
			btnIrOFF.UseVisualStyleBackColor = true;
			btnIrOFF.Click += btnIrOFF_Click;
			// 
			// groupBox11
			// 
			groupBox11.Controls.Add(btnHdmiON);
			groupBox11.Controls.Add(btnHdmiOFF);
			groupBox11.Location = new Point(1165, 22);
			groupBox11.Name = "groupBox11";
			groupBox11.Size = new Size(73, 200);
			groupBox11.TabIndex = 13;
			groupBox11.TabStop = false;
			groupBox11.Text = "HDMI";
			// 
			// btnHdmiON
			// 
			btnHdmiON.Location = new Point(15, 44);
			btnHdmiON.Name = "btnHdmiON";
			btnHdmiON.Size = new Size(40, 22);
			btnHdmiON.TabIndex = 11;
			btnHdmiON.Text = "ON";
			btnHdmiON.UseVisualStyleBackColor = true;
			btnHdmiON.Click += btnHdmiON_Click;
			// 
			// btnHdmiOFF
			// 
			btnHdmiOFF.Location = new Point(15, 72);
			btnHdmiOFF.Name = "btnHdmiOFF";
			btnHdmiOFF.Size = new Size(40, 22);
			btnHdmiOFF.TabIndex = 12;
			btnHdmiOFF.Text = "OFF";
			btnHdmiOFF.UseVisualStyleBackColor = true;
			btnHdmiOFF.Click += btnHdmiOFF_Click;
			// 
			// groupBox12
			// 
			groupBox12.Controls.Add(textBox2);
			groupBox12.Controls.Add(label12);
			groupBox12.Controls.Add(label11);
			groupBox12.Controls.Add(textBox1);
			groupBox12.Controls.Add(groupBox13);
			groupBox12.Controls.Add(btn485Conn);
			groupBox12.Controls.Add(btn485Disconn);
			groupBox12.Location = new Point(1244, 22);
			groupBox12.Name = "groupBox12";
			groupBox12.Size = new Size(122, 274);
			groupBox12.TabIndex = 14;
			groupBox12.TabStop = false;
			groupBox12.Text = "RS485";
			// 
			// textBox2
			// 
			textBox2.Location = new Point(52, 67);
			textBox2.Name = "textBox2";
			textBox2.Size = new Size(55, 23);
			textBox2.TabIndex = 17;
			// 
			// label12
			// 
			label12.AutoSize = true;
			label12.Location = new Point(7, 71);
			label12.Name = "label12";
			label12.Size = new Size(41, 15);
			label12.TabIndex = 16;
			label12.Text = "Baud :";
			// 
			// label11
			// 
			label11.AutoSize = true;
			label11.Location = new Point(6, 38);
			label11.Name = "label11";
			label11.Size = new Size(42, 15);
			label11.TabIndex = 15;
			label11.Text = "COM :";
			// 
			// textBox1
			// 
			textBox1.Location = new Point(52, 34);
			textBox1.Name = "textBox1";
			textBox1.Size = new Size(55, 23);
			textBox1.TabIndex = 14;
			// 
			// groupBox13
			// 
			groupBox13.Controls.Add(btn485Relay1OFF);
			groupBox13.Controls.Add(btn485Relay1ON);
			groupBox13.Location = new Point(6, 178);
			groupBox13.Name = "groupBox13";
			groupBox13.Size = new Size(110, 84);
			groupBox13.TabIndex = 13;
			groupBox13.TabStop = false;
			groupBox13.Text = "Relay1";
			// 
			// btn485Relay1OFF
			// 
			btn485Relay1OFF.Location = new Point(35, 51);
			btn485Relay1OFF.Name = "btn485Relay1OFF";
			btn485Relay1OFF.Size = new Size(40, 22);
			btn485Relay1OFF.TabIndex = 12;
			btn485Relay1OFF.Text = "OFF";
			btn485Relay1OFF.UseVisualStyleBackColor = true;
			btn485Relay1OFF.Click += btn485Relay1OFF_Click;
			// 
			// btn485Relay1ON
			// 
			btn485Relay1ON.Location = new Point(35, 23);
			btn485Relay1ON.Name = "btn485Relay1ON";
			btn485Relay1ON.Size = new Size(40, 22);
			btn485Relay1ON.TabIndex = 11;
			btn485Relay1ON.Text = "ON";
			btn485Relay1ON.UseVisualStyleBackColor = true;
			btn485Relay1ON.Click += btn485Relay1ON_Click;
			// 
			// btn485Conn
			// 
			btn485Conn.Location = new Point(15, 109);
			btn485Conn.Name = "btn485Conn";
			btn485Conn.Size = new Size(92, 22);
			btn485Conn.TabIndex = 11;
			btn485Conn.Text = "Connect";
			btn485Conn.UseVisualStyleBackColor = true;
			btn485Conn.Click += btn485Conn_Click;
			// 
			// btn485Disconn
			// 
			btn485Disconn.Location = new Point(15, 137);
			btn485Disconn.Name = "btn485Disconn";
			btn485Disconn.Size = new Size(92, 22);
			btn485Disconn.TabIndex = 12;
			btn485Disconn.Text = "Disconnect";
			btn485Disconn.UseVisualStyleBackColor = true;
			btn485Disconn.Click += btn485Disconn_Click;
			// 
			// textBox5
			// 
			textBox5.Location = new Point(389, 235);
			textBox5.Multiline = true;
			textBox5.Name = "textBox5";
			textBox5.ReadOnly = true;
			textBox5.Size = new Size(612, 373);
			textBox5.TabIndex = 15;
			textBox5.Text = "- 테스트 순서";
			// 
			// Form1
			// 
			AutoScaleDimensions = new SizeF(7F, 15F);
			AutoScaleMode = AutoScaleMode.Font;
			ClientSize = new Size(1380, 619);
			Controls.Add(textBox5);
			Controls.Add(groupBox12);
			Controls.Add(groupBox11);
			Controls.Add(groupBox10);
			Controls.Add(groupBox8);
			Controls.Add(groupBox7);
			Controls.Add(groupBox4);
			Controls.Add(groupBox1);
			Name = "Form1";
			Text = "Remote Deck 제어";
			FormClosing += Form1_FormClosing;
			Load += Form1_Load;
			groupBox1.ResumeLayout(false);
			groupBox1.PerformLayout();
			groupBox3.ResumeLayout(false);
			groupBox2.ResumeLayout(false);
			groupBox2.PerformLayout();
			groupBox4.ResumeLayout(false);
			groupBox6.ResumeLayout(false);
			groupBox5.ResumeLayout(false);
			groupBox7.ResumeLayout(false);
			groupBox7.PerformLayout();
			groupBox8.ResumeLayout(false);
			groupBox9.ResumeLayout(false);
			groupBox10.ResumeLayout(false);
			groupBox11.ResumeLayout(false);
			groupBox12.ResumeLayout(false);
			groupBox12.PerformLayout();
			groupBox13.ResumeLayout(false);
			ResumeLayout(false);
			PerformLayout();
		}

		#endregion

		private GroupBox groupBox1;
		private Button btnConfigSave;
		private Label label2;
		private TextBox txtPort;
		private Label label1;
		private TextBox txtIp;
		private GroupBox groupBox2;
		private Label label3;
		private TextBox txtPublish;
		private GroupBox groupBox3;
		private Button btnMqttDiscon;
		private Button btnMqttConn;
		private Label label4;
		private TextBox txtSubcribe;
		private Button btnClearLog;
		private RichTextBox richLogger;
		private GroupBox groupBox4;
		private GroupBox groupBox5;
		private Button btnRelay1ON;
		private GroupBox groupBox6;
		private Button btnRelay2OFF;
		private Button btnRelay2ON;
		private Button btnRelay1OFF;
		private GroupBox groupBox7;
		private Button btnIO1OFF;
		private Button btnIO1ON;
		private Label label5;
		private Label label13;
		private Button btnIO7OFF;
		private Button btnIO7ON;
		private Label label8;
		private Button btnIO6OFF;
		private Button btnIO6ON;
		private Label label9;
		private Button btnIO5OFF;
		private Button btnIO5ON;
		private Label label10;
		private Button btnIO4OFF;
		private Button btnIO4ON;
		private Label label7;
		private Button btnIO3OFF;
		private Label label6;
		private Button btnIO2OFF;
		private Button btnIO2ON;
		private GroupBox groupBox8;
		private Button btnLedOFF;
		private Button btnLedON;
		private Button btnGetLed;
		private GroupBox groupBox9;
		private GroupBox groupBox10;
		private Button btnIrON;
		private Button btnIrOFF;
		private GroupBox groupBox11;
		private Button btnHdmiON;
		private Button btnHdmiOFF;
		private GroupBox groupBox12;
		private Button btn485Conn;
		private Button btn485Disconn;
		private GroupBox groupBox13;
		private Button btn485Relay1OFF;
		private Button btn485Relay1ON;
		private Button btnIO3ON;
		private Button btnGetAllStatus;
		private TextBox textBox5;
		private Label label11;
		private TextBox textBox1;
		private TextBox textBox2;
		private Label label12;
	}
}
