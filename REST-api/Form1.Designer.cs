namespace REST_api
{
	partial class Form1
	{
		/// <summary>
		/// 필수 디자이너 변수입니다.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// 사용 중인 모든 리소스를 정리합니다.
		/// </summary>
		/// <param name="disposing">관리되는 리소스를 삭제해야 하면 true이고, 그렇지 않으면 false입니다.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form 디자이너에서 생성한 코드

		/// <summary>
		/// 디자이너 지원에 필요한 메서드입니다. 
		/// 이 메서드의 내용을 코드 편집기로 수정하지 마세요.
		/// </summary>
		private void InitializeComponent()
		{
			this.label1 = new System.Windows.Forms.Label();
			this.textIP = new System.Windows.Forms.TextBox();
			this.textPort = new System.Windows.Forms.TextBox();
			this.label2 = new System.Windows.Forms.Label();
			this.textPW = new System.Windows.Forms.TextBox();
			this.label3 = new System.Windows.Forms.Label();
			this.textID = new System.Windows.Forms.TextBox();
			this.label4 = new System.Windows.Forms.Label();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.btnRelay1Pluse = new System.Windows.Forms.Button();
			this.btnRelay1Off = new System.Windows.Forms.Button();
			this.btnRelay1On = new System.Windows.Forms.Button();
			this.groupBox2 = new System.Windows.Forms.GroupBox();
			this.btnRelay2Pluse = new System.Windows.Forms.Button();
			this.btnRelay2Off = new System.Windows.Forms.Button();
			this.btnRelay2On = new System.Windows.Forms.Button();
			this.textLog = new System.Windows.Forms.TextBox();
			this.label5 = new System.Windows.Forms.Label();
			this.btnConn = new System.Windows.Forms.Button();
			this.groupBox1.SuspendLayout();
			this.groupBox2.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(57, 45);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(24, 12);
			this.label1.TabIndex = 0;
			this.label1.Text = "IP :";
			// 
			// textIP
			// 
			this.textIP.Location = new System.Drawing.Point(87, 41);
			this.textIP.Name = "textIP";
			this.textIP.Size = new System.Drawing.Size(100, 21);
			this.textIP.TabIndex = 1;
			// 
			// textPort
			// 
			this.textPort.Location = new System.Drawing.Point(87, 77);
			this.textPort.Name = "textPort";
			this.textPort.Size = new System.Drawing.Size(71, 21);
			this.textPort.TabIndex = 3;
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(46, 81);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(35, 12);
			this.label2.TabIndex = 2;
			this.label2.Text = "Port :";
			// 
			// textPW
			// 
			this.textPW.Location = new System.Drawing.Point(87, 149);
			this.textPW.Name = "textPW";
			this.textPW.Size = new System.Drawing.Size(71, 21);
			this.textPW.TabIndex = 7;
			// 
			// label3
			// 
			this.label3.AutoSize = true;
			this.label3.Location = new System.Drawing.Point(50, 153);
			this.label3.Name = "label3";
			this.label3.Size = new System.Drawing.Size(31, 12);
			this.label3.TabIndex = 6;
			this.label3.Text = "PW :";
			// 
			// textID
			// 
			this.textID.Location = new System.Drawing.Point(87, 113);
			this.textID.Name = "textID";
			this.textID.Size = new System.Drawing.Size(71, 21);
			this.textID.TabIndex = 5;
			// 
			// label4
			// 
			this.label4.AutoSize = true;
			this.label4.Location = new System.Drawing.Point(57, 117);
			this.label4.Name = "label4";
			this.label4.Size = new System.Drawing.Size(24, 12);
			this.label4.TabIndex = 4;
			this.label4.Text = "ID :";
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.btnRelay1Pluse);
			this.groupBox1.Controls.Add(this.btnRelay1Off);
			this.groupBox1.Controls.Add(this.btnRelay1On);
			this.groupBox1.Location = new System.Drawing.Point(233, 41);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(245, 144);
			this.groupBox1.TabIndex = 8;
			this.groupBox1.TabStop = false;
			this.groupBox1.Text = "RELAY1";
			// 
			// btnRelay1Pluse
			// 
			this.btnRelay1Pluse.Location = new System.Drawing.Point(51, 101);
			this.btnRelay1Pluse.Name = "btnRelay1Pluse";
			this.btnRelay1Pluse.Size = new System.Drawing.Size(144, 23);
			this.btnRelay1Pluse.TabIndex = 2;
			this.btnRelay1Pluse.Text = "Pulse";
			this.btnRelay1Pluse.UseVisualStyleBackColor = true;
			// 
			// btnRelay1Off
			// 
			this.btnRelay1Off.Location = new System.Drawing.Point(51, 65);
			this.btnRelay1Off.Name = "btnRelay1Off";
			this.btnRelay1Off.Size = new System.Drawing.Size(144, 23);
			this.btnRelay1Off.TabIndex = 1;
			this.btnRelay1Off.Text = "OFF";
			this.btnRelay1Off.UseVisualStyleBackColor = true;
			// 
			// btnRelay1On
			// 
			this.btnRelay1On.Location = new System.Drawing.Point(51, 29);
			this.btnRelay1On.Name = "btnRelay1On";
			this.btnRelay1On.Size = new System.Drawing.Size(144, 23);
			this.btnRelay1On.TabIndex = 0;
			this.btnRelay1On.Text = "ON";
			this.btnRelay1On.UseVisualStyleBackColor = true;
			// 
			// groupBox2
			// 
			this.groupBox2.Controls.Add(this.btnRelay2Pluse);
			this.groupBox2.Controls.Add(this.btnRelay2Off);
			this.groupBox2.Controls.Add(this.btnRelay2On);
			this.groupBox2.Location = new System.Drawing.Point(509, 41);
			this.groupBox2.Name = "groupBox2";
			this.groupBox2.Size = new System.Drawing.Size(245, 144);
			this.groupBox2.TabIndex = 9;
			this.groupBox2.TabStop = false;
			this.groupBox2.Text = "RELAY2";
			// 
			// btnRelay2Pluse
			// 
			this.btnRelay2Pluse.Location = new System.Drawing.Point(55, 101);
			this.btnRelay2Pluse.Name = "btnRelay2Pluse";
			this.btnRelay2Pluse.Size = new System.Drawing.Size(144, 23);
			this.btnRelay2Pluse.TabIndex = 5;
			this.btnRelay2Pluse.Text = "Pulse";
			this.btnRelay2Pluse.UseVisualStyleBackColor = true;
			// 
			// btnRelay2Off
			// 
			this.btnRelay2Off.Location = new System.Drawing.Point(55, 65);
			this.btnRelay2Off.Name = "btnRelay2Off";
			this.btnRelay2Off.Size = new System.Drawing.Size(144, 23);
			this.btnRelay2Off.TabIndex = 4;
			this.btnRelay2Off.Text = "OFF";
			this.btnRelay2Off.UseVisualStyleBackColor = true;
			// 
			// btnRelay2On
			// 
			this.btnRelay2On.Location = new System.Drawing.Point(55, 29);
			this.btnRelay2On.Name = "btnRelay2On";
			this.btnRelay2On.Size = new System.Drawing.Size(144, 23);
			this.btnRelay2On.TabIndex = 3;
			this.btnRelay2On.Text = "ON";
			this.btnRelay2On.UseVisualStyleBackColor = true;
			// 
			// textLog
			// 
			this.textLog.Location = new System.Drawing.Point(53, 260);
			this.textLog.Multiline = true;
			this.textLog.Name = "textLog";
			this.textLog.ReadOnly = true;
			this.textLog.Size = new System.Drawing.Size(695, 178);
			this.textLog.TabIndex = 10;
			// 
			// label5
			// 
			this.label5.AutoSize = true;
			this.label5.Location = new System.Drawing.Point(57, 245);
			this.label5.Name = "label5";
			this.label5.Size = new System.Drawing.Size(61, 12);
			this.label5.TabIndex = 11;
			this.label5.Text = "STATUS :";
			// 
			// btnConn
			// 
			this.btnConn.Location = new System.Drawing.Point(48, 187);
			this.btnConn.Name = "btnConn";
			this.btnConn.Size = new System.Drawing.Size(139, 23);
			this.btnConn.TabIndex = 3;
			this.btnConn.Text = "Start";
			this.btnConn.UseVisualStyleBackColor = true;
			// 
			// Form1
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 12F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(800, 450);
			this.Controls.Add(this.btnConn);
			this.Controls.Add(this.label5);
			this.Controls.Add(this.textLog);
			this.Controls.Add(this.groupBox2);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.textPW);
			this.Controls.Add(this.label3);
			this.Controls.Add(this.textID);
			this.Controls.Add(this.label4);
			this.Controls.Add(this.textPort);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.textIP);
			this.Controls.Add(this.label1);
			this.Name = "Form1";
			this.Text = "Form1";
			this.groupBox1.ResumeLayout(false);
			this.groupBox2.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox textIP;
		private System.Windows.Forms.TextBox textPort;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox textPW;
		private System.Windows.Forms.Label label3;
		private System.Windows.Forms.TextBox textID;
		private System.Windows.Forms.Label label4;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.Button btnRelay1Pluse;
		private System.Windows.Forms.Button btnRelay1Off;
		private System.Windows.Forms.Button btnRelay1On;
		private System.Windows.Forms.GroupBox groupBox2;
		private System.Windows.Forms.Button btnRelay2Pluse;
		private System.Windows.Forms.Button btnRelay2Off;
		private System.Windows.Forms.Button btnRelay2On;
		private System.Windows.Forms.TextBox textLog;
		private System.Windows.Forms.Label label5;
		private System.Windows.Forms.Button btnConn;
	}
}

