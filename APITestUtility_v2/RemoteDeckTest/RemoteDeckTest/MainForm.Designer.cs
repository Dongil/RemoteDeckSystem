namespace RemoteDeckTest;

partial class MainForm
{
    private System.ComponentModel.IContainer components = null;

    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null)) components.Dispose();
        base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    private void InitializeComponent()
    {
        // ─── Form ───
        this.Text = "RemoteDeck v2.2 API Test Utility";
        this.Size = new Size(720, 750);
        this.StartPosition = FormStartPosition.CenterScreen;
        this.Font = new Font("Segoe UI", 9F);

        var tabMain = new TabControl { Dock = DockStyle.Fill };

        // ═══════════════════════════════════════════════
        //  TAB 1: Connection Settings
        // ═══════════════════════════════════════════════
        var tabConn = new TabPage("Connection");
        var pnlConn = new Panel { Dock = DockStyle.Fill, AutoScroll = true, Padding = new Padding(10) };
        int gw = 660; // group width
        int tw = 280; // textbox width

        // --- Device (HTTP/WS) ---
        var grpDevice = new GroupBox { Text = "Device (HTTP / WebSocket)", Location = new Point(10, 10), Width = gw, Height = 155 };
        int dy = 22;
        grpDevice.Controls.Add(new Label { Text = "IP:", Location = new Point(10, dy + 3), AutoSize = true });
        txtIP = new TextBox { Text = "192.168.10.196", Location = new Point(80, dy), Width = tw };
        grpDevice.Controls.Add(txtIP);
        grpDevice.Controls.Add(new Label { Text = "Port:", Location = new Point(370, dy + 3), AutoSize = true });
        txtWebPort = new TextBox { Text = "5050", Location = new Point(410, dy), Width = 60 };
        grpDevice.Controls.Add(txtWebPort);
        dy += 30;
        grpDevice.Controls.Add(new Label { Text = "User:", Location = new Point(10, dy + 3), AutoSize = true });
        txtUser = new TextBox { Text = "admin", Location = new Point(80, dy), Width = 120 };
        grpDevice.Controls.Add(txtUser);
        grpDevice.Controls.Add(new Label { Text = "Pass:", Location = new Point(210, dy + 3), AutoSize = true });
        txtPass = new TextBox { Text = "12345", Location = new Point(250, dy), Width = 120 };
        grpDevice.Controls.Add(txtPass);
        dy += 35;
        btnHttpStatus = new Button { Text = "HTTP Status", Location = new Point(10, dy), Width = 100 };
        btnHttpStatus.Click += btnHttpStatus_Click;
        grpDevice.Controls.Add(btnHttpStatus);
        btnHttpConfig = new Button { Text = "HTTP Config", Location = new Point(120, dy), Width = 100 };
        btnHttpConfig.Click += btnHttpConfig_Click;
        grpDevice.Controls.Add(btnHttpConfig);
        btnWsConnect = new Button { Text = "WS Connect", Location = new Point(230, dy), Width = 110 };
        btnWsConnect.Click += btnWsConnect_Click;
        grpDevice.Controls.Add(btnWsConnect);
        pnlConn.Controls.Add(grpDevice);

        // --- MQTT ---
        var grpMqtt = new GroupBox { Text = "MQTT", Location = new Point(10, 172), Width = gw, Height = 185 };
        dy = 22;
        grpMqtt.Controls.Add(new Label { Text = "Broker:", Location = new Point(10, dy + 3), AutoSize = true });
        txtMqttBroker = new TextBox { Text = "192.168.10.100", Location = new Point(80, dy), Width = tw };
        grpMqtt.Controls.Add(txtMqttBroker);
        grpMqtt.Controls.Add(new Label { Text = "Port:", Location = new Point(370, dy + 3), AutoSize = true });
        txtMqttPort = new TextBox { Text = "1883", Location = new Point(410, dy), Width = 60 };
        grpMqtt.Controls.Add(txtMqttPort);
        dy += 30;
        grpMqtt.Controls.Add(new Label { Text = "User:", Location = new Point(10, dy + 3), AutoSize = true });
        txtMqttUser = new TextBox { Location = new Point(80, dy), Width = 120 };
        grpMqtt.Controls.Add(txtMqttUser);
        grpMqtt.Controls.Add(new Label { Text = "Pass:", Location = new Point(210, dy + 3), AutoSize = true });
        txtMqttPass = new TextBox { Location = new Point(250, dy), Width = 120 };
        grpMqtt.Controls.Add(txtMqttPass);
        dy += 30;
        grpMqtt.Controls.Add(new Label { Text = "Pub Topic:", Location = new Point(10, dy + 3), AutoSize = true });
        txtMqttPubTopic = new TextBox { Text = "RemoteDeck/PC/node_1", Location = new Point(80, dy), Width = tw };
        grpMqtt.Controls.Add(txtMqttPubTopic);
        dy += 30;
        grpMqtt.Controls.Add(new Label { Text = "Sub Topic:", Location = new Point(10, dy + 3), AutoSize = true });
        txtMqttSubTopic = new TextBox { Text = "RemoteDeck/PC/server", Location = new Point(80, dy), Width = tw };
        grpMqtt.Controls.Add(txtMqttSubTopic);
        dy += 30;
        btnMqttConnect = new Button { Text = "Connect", Location = new Point(10, dy), Width = 100 };
        btnMqttConnect.Click += btnMqttConnect_Click;
        grpMqtt.Controls.Add(btnMqttConnect);
        pnlConn.Controls.Add(grpMqtt);

        // --- Serial ---
        var grpSerial = new GroupBox { Text = "RS485 (Serial)", Location = new Point(10, 364), Width = gw, Height = 65 };
        dy = 25;
        grpSerial.Controls.Add(new Label { Text = "Port:", Location = new Point(10, dy + 3), AutoSize = true });
        cboSerialPort = new ComboBox { Location = new Point(50, dy), Width = 90, DropDownStyle = ComboBoxStyle.DropDownList };
        grpSerial.Controls.Add(cboSerialPort);
        btnRefreshPorts = new Button { Text = "↻", Location = new Point(145, dy - 2), Width = 30 };
        btnRefreshPorts.Click += btnRefreshPorts_Click;
        grpSerial.Controls.Add(btnRefreshPorts);
        grpSerial.Controls.Add(new Label { Text = "Baud:", Location = new Point(190, dy + 3), AutoSize = true });
        txtBaud = new TextBox { Text = "9600", Location = new Point(230, dy), Width = 60 };
        grpSerial.Controls.Add(txtBaud);
        btnSerialConnect = new Button { Text = "Connect", Location = new Point(310, dy - 2), Width = 90 };
        btnSerialConnect.Click += btnSerialConnect_Click;
        grpSerial.Controls.Add(btnSerialConnect);
        pnlConn.Controls.Add(grpSerial);

        tabConn.Controls.Add(pnlConn);
        tabMain.TabPages.Add(tabConn);

        // ═══════════════════════════════════════════════
        //  TAB 2: Control & Monitor
        // ═══════════════════════════════════════════════
        var tabCtrl = new TabPage("Control & Monitor");
        var splitCtrl = new SplitContainer { Dock = DockStyle.Fill, Orientation = Orientation.Horizontal, SplitterDistance = 265 };

        // --- Top: Controls ---
        var pnlTop = new Panel { Dock = DockStyle.Fill };
        int cw = 660; // control area width

        // Row 1: Interface + Relay 1 + Relay 2
        var grpIface = new GroupBox { Text = "Send Interface", Location = new Point(10, 5), Width = 150, Height = 55 };
        cboInterface = new ComboBox { Location = new Point(10, 22), Width = 125, DropDownStyle = ComboBoxStyle.DropDownList };
        cboInterface.Items.AddRange(new object[] { "HTTP", "MQTT", "RS485" });
        cboInterface.SelectedIndex = 0;
        grpIface.Controls.Add(cboInterface);
        pnlTop.Controls.Add(grpIface);

        var grpR1 = new GroupBox { Text = "Relay 1", Location = new Point(168, 5), Width = 225, Height = 55 };
        btnRelay1On = new Button { Text = "ON", Location = new Point(10, 20), Width = 60 };
        btnRelay1On.Click += btnRelay1On_Click;
        grpR1.Controls.Add(btnRelay1On);
        btnRelay1Off = new Button { Text = "OFF", Location = new Point(75, 20), Width = 60 };
        btnRelay1Off.Click += btnRelay1Off_Click;
        grpR1.Controls.Add(btnRelay1Off);
        btnRelay1Pulse = new Button { Text = "Pulse", Location = new Point(140, 20), Width = 70 };
        btnRelay1Pulse.Click += btnRelay1Pulse_Click;
        grpR1.Controls.Add(btnRelay1Pulse);
        pnlTop.Controls.Add(grpR1);

        var grpR2 = new GroupBox { Text = "Relay 2", Location = new Point(400, 5), Width = 225, Height = 55 };
        btnRelay2On = new Button { Text = "ON", Location = new Point(10, 20), Width = 60 };
        btnRelay2On.Click += btnRelay2On_Click;
        grpR2.Controls.Add(btnRelay2On);
        btnRelay2Off = new Button { Text = "OFF", Location = new Point(75, 20), Width = 60 };
        btnRelay2Off.Click += btnRelay2Off_Click;
        grpR2.Controls.Add(btnRelay2Off);
        btnRelay2Pulse = new Button { Text = "Pulse", Location = new Point(140, 20), Width = 70 };
        btnRelay2Pulse.Click += btnRelay2Pulse_Click;
        grpR2.Controls.Add(btnRelay2Pulse);
        pnlTop.Controls.Add(grpR2);

        // Row 2: Commands
        var grpCmd = new GroupBox { Text = "Commands", Location = new Point(10, 62), Width = cw, Height = 55 };
        btnGetStatus = new Button { Text = "Get Status", Location = new Point(10, 20), Width = 85 };
        btnGetStatus.Click += btnGetStatus_Click;
        grpCmd.Controls.Add(btnGetStatus);
        btnWOL = new Button { Text = "WOL", Location = new Point(100, 20), Width = 60 };
        btnWOL.Click += btnWOL_Click;
        grpCmd.Controls.Add(btnWOL);
        btnReboot = new Button { Text = "Reboot", Location = new Point(165, 20), Width = 70, ForeColor = Color.Red };
        btnReboot.Click += btnReboot_Click;
        grpCmd.Controls.Add(btnReboot);
        btnClearLog = new Button { Text = "Clear Log", Location = new Point(560, 20), Width = 80 };
        btnClearLog.Click += btnClearLog_Click;
        grpCmd.Controls.Add(btnClearLog);
        pnlTop.Controls.Add(grpCmd);

        // Row 3: IO Monitor
        var grpMon = new GroupBox { Text = "IO Status Monitor", Location = new Point(10, 120), Width = cw, Height = 80 };
        int lx = 10;
        int indW = (cw - 30) / 6;
        Label MakeIndicator(string name, int x)
        {
            grpMon.Controls.Add(new Label { Text = name, Location = new Point(x, 18), AutoSize = true, Font = new Font("Segoe UI", 8F, FontStyle.Bold) });
            var lbl = new Label { Text = "OFF", Location = new Point(x, 38), Width = indW - 8, Height = 28,
                BackColor = Color.DarkRed, ForeColor = Color.White, TextAlign = ContentAlignment.MiddleCenter,
                Font = new Font("Segoe UI", 10F, FontStyle.Bold) };
            grpMon.Controls.Add(lbl);
            return lbl;
        }
        lblRelay1 = MakeIndicator("Relay 1", lx); lx += indW;
        lblRelay2 = MakeIndicator("Relay 2", lx); lx += indW;
        lblPCLed  = MakeIndicator("PC-LED", lx);  lx += indW;
        lblGPIO1  = MakeIndicator("GPIO 1", lx);  lx += indW;
        lblGPIO2  = MakeIndicator("GPIO 2", lx);  lx += indW;
        lblGPIO3  = MakeIndicator("GPIO 3", lx);
        pnlTop.Controls.Add(grpMon);

        // Row 4: Custom JSON
        var grpCustom = new GroupBox { Text = "Custom JSON Command", Location = new Point(10, 203), Width = cw, Height = 55 };
        txtCustomJson = new TextBox { Text = "{\"cmd\":\"status\"}", Location = new Point(10, 22), Width = cw - 100 };
        grpCustom.Controls.Add(txtCustomJson);
        var btnSendCustom = new Button { Text = "Send", Location = new Point(cw - 80, 20), Width = 65 };
        btnSendCustom.Click += async (s, e) => await SendCommand(txtCustomJson.Text);
        grpCustom.Controls.Add(btnSendCustom);
        pnlTop.Controls.Add(grpCustom);

        splitCtrl.Panel1.Controls.Add(pnlTop);

        // --- Bottom: Log ---
        txtLog = new TextBox { Dock = DockStyle.Fill, Multiline = true, ScrollBars = ScrollBars.Vertical,
            ReadOnly = true, BackColor = Color.FromArgb(20, 20, 30), ForeColor = Color.LimeGreen,
            Font = new Font("Consolas", 9F) };
        splitCtrl.Panel2.Controls.Add(txtLog);

        tabCtrl.Controls.Add(splitCtrl);
        tabMain.TabPages.Add(tabCtrl);

        this.Controls.Add(tabMain);

        // Init serial ports
        btnRefreshPorts_Click(null, EventArgs.Empty);
    }

    #endregion

    // ─── Controls ───
    private TextBox txtIP = null!;
    private TextBox txtWebPort = null!;
    private TextBox txtUser = null!;
    private TextBox txtPass = null!;
    private Button btnHttpStatus = null!;
    private Button btnHttpConfig = null!;
    private Button btnWsConnect = null!;

    private TextBox txtMqttBroker = null!;
    private TextBox txtMqttPort = null!;
    private TextBox txtMqttUser = null!;
    private TextBox txtMqttPass = null!;
    private TextBox txtMqttPubTopic = null!;
    private TextBox txtMqttSubTopic = null!;
    private Button btnMqttConnect = null!;

    private ComboBox cboSerialPort = null!;
    private TextBox txtBaud = null!;
    private Button btnSerialConnect = null!;
    private Button btnRefreshPorts = null!;

    private ComboBox cboInterface = null!;
    private Button btnRelay1On = null!;
    private Button btnRelay1Off = null!;
    private Button btnRelay1Pulse = null!;
    private Button btnRelay2On = null!;
    private Button btnRelay2Off = null!;
    private Button btnRelay2Pulse = null!;
    private Button btnGetStatus = null!;
    private Button btnWOL = null!;
    private Button btnReboot = null!;
    private Button btnClearLog = null!;
    private TextBox txtCustomJson = null!;

    private Label lblRelay1 = null!;
    private Label lblRelay2 = null!;
    private Label lblPCLed = null!;
    private Label lblGPIO1 = null!;
    private Label lblGPIO2 = null!;
    private Label lblGPIO3 = null!;

    private TextBox txtLog = null!;
}
