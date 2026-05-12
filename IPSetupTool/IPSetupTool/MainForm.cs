using System.Net;
using IPSetupTool.Models;
using IPSetupTool.Services;

namespace IPSetupTool;

public class MainForm : Form
{
    private readonly UDPDiscoveryService _discovery = new();
    private readonly DeviceConfigService _configService = new();

    private DataGridView _grid = null!;
    private List<DeviceInfo> _devices = new();
    private DeviceInfo? _selectedDevice;
    private bool _connected = false;
    private string? _tempIPNic = null;

    // Device info
    private TextBox _txtDeviceId = null!, _txtDeviceName = null!;
    private ComboBox _cboNetMode = null!;

    // Ethernet
    private TextBox _txtEthIp = null!, _txtEthGateway = null!, _txtEthSubnet = null!, _txtEthDns = null!;
    private CheckBox _chkEthDhcp = null!;

    // WiFi STA
    private TextBox _txtWifiSsid = null!, _txtWifiPassword = null!,
                    _txtWifiIp = null!, _txtWifiGateway = null!, _txtWifiSubnet = null!, _txtWifiDns = null!;
    private CheckBox _chkWifiDhcp = null!;

    private Button _btnConnect = null!, _btnSave = null!, _btnTest = null!, _btnWeb = null!;
    private Label _lblStatus = null!;
    private ProgressBar _progressBar = null!;
    private Panel _settingsPanel = null!;

    public MainForm()
    {
        Text = "RemoteDeck IP 설정 도구 v2.1";
        Size = new Size(620, 800);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        BuildUI();
        FormClosing += (_, _) => CleanupTempIP();
        Shown += (_, _) => RecoverStaleNicState();
    }

    private void RecoverStaleNicState()
    {
        var stale = NetworkHelper.LoadNicState();
        if (stale == null) return;
        if (!stale.WasDhcp && !stale.WasDnsAuto)
        {
            // Nothing to restore (was static originally) — just clean up state file
            NetworkHelper.ClearNicState();
            return;
        }
        var ageMin = (DateTime.Now - stale.SavedAt).TotalMinutes;
        var msg = $"이전 실행에서 네트워크 설정이 변경된 후 정상 종료되지 않은 것 같습니다.\n\n" +
                  $"NIC: {stale.NicName}\n" +
                  $"원래 DHCP: {(stale.WasDhcp ? "예" : "아니오")}\n" +
                  $"원래 DNS 자동: {(stale.WasDnsAuto ? "예" : "아니오")}\n" +
                  $"기록 시각: {stale.SavedAt:yyyy-MM-dd HH:mm:ss} ({ageMin:F0}분 전)\n\n" +
                  $"DHCP 모드로 복원하시겠습니까?";
        var result = MessageBox.Show(this, msg, "네트워크 상태 복원", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (result == DialogResult.Yes)
        {
            NetworkHelper.RemoveTempIP(stale.NicName); // also removes leftover *.100 + restores DHCP via internal restore call
            SetStatus($"{stale.NicName}을(를) DHCP로 복원했습니다.", false);
        }
        else
        {
            NetworkHelper.ClearNicState();
        }
    }

    private void BuildUI()
    {
        var y = 10;

        var btnDiscover = new Button { Text = "기기 검색", Location = new Point(10, y), Size = new Size(130, 30) };
        btnDiscover.Click += async (_, _) => await SmartDiscover();
        Controls.Add(btnDiscover);
        y += 35;

        _grid = new DataGridView
        {
            Location = new Point(10, y), Size = new Size(580, 100),
            ReadOnly = true, SelectionMode = DataGridViewSelectionMode.FullRowSelect,
            MultiSelect = false, AllowUserToAddRows = false,
            AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
        };
        _grid.Columns.Add("DeviceId", "장치 ID");
        _grid.Columns.Add("IP", "IP 주소");
        _grid.Columns.Add("MAC", "MAC");
        _grid.Columns.Add("FW", "FW");
        _grid.Columns.Add("Mode", "모드");
        Controls.Add(_grid);
        y += 105;

        // Connect + Manual IP
        _btnConnect = new Button { Text = "장치 연결", Location = new Point(10, y), Size = new Size(100, 28), Enabled = false };
        _btnConnect.Click += async (_, _) => await ConnectToDevice();
        Controls.Add(_btnConnect);

        Controls.Add(new Label { Text = "IP 직접입력:", Location = new Point(120, y + 4), AutoSize = true });
        var txtManualIp = new TextBox { Location = new Point(195, y + 1), Size = new Size(140, 23), PlaceholderText = "192.168.x.x" };
        Controls.Add(txtManualIp);

        var btnManualConnect = new Button { Text = "직접 연결", Location = new Point(340, y), Size = new Size(90, 28) };
        btnManualConnect.Click += async (_, _) =>
        {
            var ip = txtManualIp.Text.Trim();
            if (!ValidateIP(ip)) { MessageBox.Show("올바른 IP를 입력하세요.", "알림"); return; }
            if (!IsSameSubnet(ip, GetPrimaryIP()))
            {
                SetStatus($"{ip} 서브넷 접근을 위해 임시 IP 추가 중...", true);
                CleanupTempIP();
                var parts = ip.Split('.');
                var tempIp = $"{parts[0]}.{parts[1]}.{parts[2]}.100";
                var nics = NetworkHelper.GetPhysicalNICs();
                if (nics.Count > 0)
                {
                    NetworkHelper.AddTempIPCustom(nics[0].Name, tempIp);
                    _tempIPNic = nics[0].Name;
                    await Task.Delay(3000);
                    using var ping = new System.Net.NetworkInformation.Ping();
                    for (int i = 0; i < 5; i++)
                    { try { var r = await ping.SendPingAsync(ip, 1000); if (r.Status == System.Net.NetworkInformation.IPStatus.Success) break; } catch { } await Task.Delay(500); }
                    await Task.Delay(1000);
                }
            }
            _selectedDevice = new DeviceInfo { DeviceId = "unknown", IP = ip, WebPort = 5050 };
            _grid.Rows.Clear();
            _grid.Rows.Add("?", ip, "?", "?", "?");
            _devices = new List<DeviceInfo> { _selectedDevice };
            await ConnectToDevice();
        };
        Controls.Add(btnManualConnect);

        _grid.SelectionChanged += (_, _) =>
        {
            _btnConnect.Enabled = _grid.SelectedRows.Count > 0 && _grid.SelectedRows[0].Index < _devices.Count;
        };
        y += 35;

        // Settings Panel
        _settingsPanel = new Panel
        {
            Location = new Point(0, y), Size = new Size(610, 530), Enabled = false
        };
        Controls.Add(_settingsPanel);
        int py = 0;

        // 기기 정보
        _settingsPanel.Controls.Add(new Label { Text = "── 기기 정보 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "장치 ID:", ref _txtDeviceId, ref py, "");
        AddLabeledField(_settingsPanel, "장치 이름:", ref _txtDeviceName, ref py, "");
        py += 5;

        // 네트워크 모드
        _settingsPanel.Controls.Add(new Label { Text = "── 네트워크 모드 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        _settingsPanel.Controls.Add(new Label { Text = "모드:", Location = new Point(10, py + 3), Size = new Size(105, 20), TextAlign = ContentAlignment.MiddleRight });
        _cboNetMode = new ComboBox { Location = new Point(120, py), Size = new Size(120, 23), DropDownStyle = ComboBoxStyle.DropDownList };
        _cboNetMode.Items.AddRange(new object[] { "ethernet", "wifi" });
        _cboNetMode.SelectedIndex = 0;
        _settingsPanel.Controls.Add(_cboNetMode);
        py += 30;

        // 이더넷
        _settingsPanel.Controls.Add(new Label { Text = "── 이더넷 설정 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "IP 주소:", ref _txtEthIp, ref py, "");
        AddLabeledField(_settingsPanel, "서브넷:", ref _txtEthSubnet, ref py, "");
        AddLabeledField(_settingsPanel, "게이트웨이:", ref _txtEthGateway, ref py, "");
        AddLabeledField(_settingsPanel, "DNS:", ref _txtEthDns, ref py, "");
        _chkEthDhcp = new CheckBox { Text = "DHCP", Location = new Point(120, py), AutoSize = true };
        _chkEthDhcp.CheckedChanged += (_, _) => { _txtEthIp.Enabled = !_chkEthDhcp.Checked; _txtEthSubnet.Enabled = !_chkEthDhcp.Checked; _txtEthGateway.Enabled = !_chkEthDhcp.Checked; _txtEthDns.Enabled = !_chkEthDhcp.Checked; };
        _settingsPanel.Controls.Add(_chkEthDhcp);
        py += 28;

        // WiFi STA
        _settingsPanel.Controls.Add(new Label { Text = "── WiFi 설정 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "WiFi SSID:", ref _txtWifiSsid, ref py, "");
        AddLabeledField(_settingsPanel, "WiFi 비밀번호:", ref _txtWifiPassword, ref py, "");
        _txtWifiPassword.PasswordChar = '*';
        var chkShowPw = new CheckBox { Text = "보이기", Location = new Point(375, py - 26 + 2), AutoSize = true };
        chkShowPw.CheckedChanged += (_, _) => { _txtWifiPassword.PasswordChar = chkShowPw.Checked ? '\0' : '*'; };
        _settingsPanel.Controls.Add(chkShowPw);
        AddLabeledField(_settingsPanel, "WiFi IP:", ref _txtWifiIp, ref py, "");
        AddLabeledField(_settingsPanel, "WiFi 게이트웨이:", ref _txtWifiGateway, ref py, "");
        AddLabeledField(_settingsPanel, "WiFi 서브넷:", ref _txtWifiSubnet, ref py, "");
        AddLabeledField(_settingsPanel, "WiFi DNS:", ref _txtWifiDns, ref py, "");
        _chkWifiDhcp = new CheckBox { Text = "DHCP", Location = new Point(120, py), AutoSize = true };
        _chkWifiDhcp.CheckedChanged += (_, _) => { _txtWifiIp.Enabled = !_chkWifiDhcp.Checked; _txtWifiGateway.Enabled = !_chkWifiDhcp.Checked; _txtWifiSubnet.Enabled = !_chkWifiDhcp.Checked; _txtWifiDns.Enabled = !_chkWifiDhcp.Checked; };
        _settingsPanel.Controls.Add(_chkWifiDhcp);
        py += 30;

        // Buttons
        _btnSave = new Button { Text = "저장 && 재부팅", Location = new Point(10, py), Size = new Size(130, 35) };
        _btnSave.Click += async (_, _) => await SaveAndReboot();
        _settingsPanel.Controls.Add(_btnSave);

        _btnTest = new Button { Text = "연결 테스트", Location = new Point(150, py), Size = new Size(110, 35) };
        _btnTest.Click += async (_, _) => await TestConnection();
        _settingsPanel.Controls.Add(_btnTest);

        _btnWeb = new Button { Text = "웹 UI 열기", Location = new Point(270, py), Size = new Size(100, 35) };
        _btnWeb.Click += (_, _) => OpenWebUI();
        _settingsPanel.Controls.Add(_btnWeb);

        // Progress bar: top right, next to search button
        _progressBar = new ProgressBar { Location = new Point(150, 12), Size = new Size(440, 25), Style = ProgressBarStyle.Marquee, Visible = false };
        Controls.Add(_progressBar);

        // Status label: bottom of form
        y += _settingsPanel.Height + 5;
        _lblStatus = new Label
        {
            Text = "'기기 검색' 버튼을 눌러 시작하세요.",
            Location = new Point(10, y),
            Size = new Size(580, 35),
            ForeColor = Color.FromArgb(0, 200, 255),
            BackColor = Color.FromArgb(30, 30, 60),
            Font = new Font(Font.FontFamily, 12f),
            TextAlign = ContentAlignment.MiddleLeft,
            Padding = new Padding(8, 0, 0, 0)
        };
        Controls.Add(_lblStatus);
    }

    private void AddLabeledField(Control parent, string label, ref TextBox textBox, ref int y, string defaultValue)
    {
        parent.Controls.Add(new Label { Text = label, Location = new Point(10, y + 3), Size = new Size(105, 20), TextAlign = ContentAlignment.MiddleRight });
        textBox = new TextBox { Location = new Point(120, y), Size = new Size(250, 23), Text = defaultValue };
        parent.Controls.Add(textBox);
        y += 26;
    }

    // ─── Smart Discover ────────────────────────────────────────

    private async Task SmartDiscover()
    {
        SetStatus("기기 검색 중...", true);
        _grid.Rows.Clear(); _connected = false; _settingsPanel.Enabled = false; ClearFields();
        EnsureFirewallRule();

        _devices = await _discovery.DiscoverDevicesAsync();
        if (_devices.Count > 0) { ShowDevices(); SetStatus($"{_devices.Count}대의 기기를 발견했습니다.", false); return; }

        if (!NetworkHelper.HasSubnet192168_1())
        {
            var answer = MessageBox.Show("기기를 찾을 수 없습니다.\n임시 IP를 추가하여 재검색하시겠습니까?",
                "기기 검색", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
            if (answer != DialogResult.Yes) { SetStatus("검색 취소됨.", false); return; }

            var nics = NetworkHelper.GetPhysicalNICs();
            string? nicName = nics.Count == 1 ? nics[0].Name : SelectNIC(nics);
            if (nicName == null) return;

            var (added, _) = NetworkHelper.AddTempIP(nicName);
            if (!added) { MessageBox.Show("임시 IP 추가 실패. 관리자 권한으로 실행하세요.", "오류"); return; }
            _tempIPNic = nicName;

            SetStatus("임시 IP 적용 및 ARP 확인 중...", true);
            await Task.Delay(3000);
            using var ping = new System.Net.NetworkInformation.Ping();
            for (int i = 0; i < 5; i++) { try { var r = await ping.SendPingAsync("192.168.1.200", 1000); if (r.Status == System.Net.NetworkInformation.IPStatus.Success) break; } catch { } await Task.Delay(500); }
            await Task.Delay(1000);

            for (int retry = 1; retry <= 3; retry++)
            {
                SetStatus($"기기 재검색 중... ({retry}/3)", true);
                _devices = await _discovery.DiscoverDevicesAsync();
                if (_devices.Count > 0) break;
                await Task.Delay(2000);
            }
        }

        if (_devices.Count > 0) { ShowDevices(); SetStatus($"{_devices.Count}대 발견.", false); }
        else SetStatus("기기를 찾을 수 없습니다.", false);
    }

    private string? SelectNIC(List<(string Name, string Description, List<string> IPs, int Index)> nics)
    {
        var f = new Form { Text = "네트워크 어댑터 선택", Size = new Size(500, 230), StartPosition = FormStartPosition.CenterParent, FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false };
        var lb = new ListBox { Location = new Point(15, 15), Size = new Size(450, 120) };
        foreach (var n in nics) lb.Items.Add($"{n.Name} ({n.Description}) [{string.Join(",", n.IPs)}]");
        lb.SelectedIndex = 0; f.Controls.Add(lb);
        var ok = new Button { Text = "확인", Location = new Point(300, 150), Size = new Size(80, 30), DialogResult = DialogResult.OK };
        var cancel = new Button { Text = "취소", Location = new Point(385, 150), Size = new Size(80, 30), DialogResult = DialogResult.Cancel };
        f.Controls.Add(ok); f.Controls.Add(cancel); f.AcceptButton = ok; f.CancelButton = cancel;
        return f.ShowDialog(this) == DialogResult.OK ? nics[lb.SelectedIndex].Name : null;
    }

    private void ShowDevices()
    {
        _grid.Rows.Clear();
        foreach (var d in _devices)
            _grid.Rows.Add(d.DeviceId, d.IP, d.MAC, d.FirmwareVersion, d.NetMode ?? "?");
        _btnConnect.Enabled = _devices.Count > 0;
    }

    private void CleanupTempIP() { if (_tempIPNic != null) { NetworkHelper.RemoveTempIP(_tempIPNic); _tempIPNic = null; } }

    // ─── Connect ───────────────────────────────────────────────

    private async Task ConnectToDevice()
    {
        if (_grid.SelectedRows.Count == 0) return;
        int idx = _grid.SelectedRows[0].Index;
        if (idx < 0 || idx >= _devices.Count) return;

        _selectedDevice = _devices[idx];
        var mgmtIP = _selectedDevice.ManagementIP;
        SetStatus($"{mgmtIP}에서 설정을 불러오는 중...", true);

        // 관리 IP가 다른 서브넷이면 임시 IP 추가
        if (!IsSameSubnet(mgmtIP, GetPrimaryIP()))
        {
            CleanupTempIP();
            var parts = mgmtIP.Split('.');
            var tempIp = $"{parts[0]}.{parts[1]}.{parts[2]}.100";
            var nics = NetworkHelper.GetPhysicalNICs();
            if (nics.Count > 0)
            {
                NetworkHelper.AddTempIPCustom(nics[0].Name, tempIp);
                _tempIPNic = nics[0].Name;
                await Task.Delay(3000);
                using var ping = new System.Net.NetworkInformation.Ping();
                for (int i = 0; i < 5; i++)
                { try { var r = await ping.SendPingAsync(mgmtIP, 1000); if (r.Status == System.Net.NetworkInformation.IPStatus.Success) break; } catch { } await Task.Delay(500); }
                await Task.Delay(500);
            }
        }

        var mgmtDevice = new DeviceInfo { DeviceId = _selectedDevice.DeviceId, IP = mgmtIP, WebPort = _selectedDevice.WebPort };
        var config = await _discovery.GetConfigViaUDPAsync(mgmtDevice);
        if (config == null) config = await _configService.GetConfigAsync(mgmtIP, _selectedDevice.WebPort);

        if (config != null)
        {
            _selectedDevice.DeviceId = config.DeviceId;
            _txtDeviceId.Text = config.DeviceId;
            _txtDeviceName.Text = config.DeviceName ?? "새기기";
            _cboNetMode.SelectedItem = config.Network?.Mode ?? "ethernet";

            _chkEthDhcp.Checked = config.Network?.Ethernet?.Dhcp ?? false;
            _txtEthIp.Text = config.Network?.Ethernet?.IP ?? "";
            _txtEthGateway.Text = config.Network?.Ethernet?.Gateway ?? "";
            _txtEthSubnet.Text = config.Network?.Ethernet?.Subnet ?? "";
            _txtEthDns.Text = config.Network?.Ethernet?.Dns1 ?? "";

            _txtWifiSsid.Text = config.Network?.Wifi?.Ssid ?? "";
            _txtWifiPassword.Text = config.Network?.Wifi?.Password ?? "";
            _chkWifiDhcp.Checked = config.Network?.Wifi?.Dhcp ?? true;
            _txtWifiIp.Text = config.Network?.Wifi?.IP ?? "";
            _txtWifiGateway.Text = config.Network?.Wifi?.Gateway ?? "";
            _txtWifiSubnet.Text = config.Network?.Wifi?.Subnet ?? "";
            _txtWifiDns.Text = config.Network?.Wifi?.Dns1 ?? "";

            _connected = true;
            _settingsPanel.Enabled = true;
            string connInfo = _selectedDevice.EthIP != null
                ? $"{config.DeviceName ?? config.DeviceId} (관리:{mgmtIP}, 서비스:{_selectedDevice.IP})"
                : $"{config.DeviceName ?? config.DeviceId} ({_selectedDevice.IP})";
            SetStatus($"{connInfo}에 연결되었습니다.", false);
        }
        else
        {
            SetStatus($"{mgmtIP} 연결 실패.", false);
            MessageBox.Show($"{mgmtIP}에 연결할 수 없습니다.", "연결 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void ClearFields()
    {
        _txtDeviceId.Text = ""; _txtDeviceName.Text = ""; _cboNetMode.SelectedIndex = 0;
        _txtEthIp.Text = ""; _txtEthSubnet.Text = ""; _txtEthGateway.Text = ""; _txtEthDns.Text = ""; _chkEthDhcp.Checked = false;
        _txtWifiSsid.Text = ""; _txtWifiPassword.Text = ""; _txtWifiIp.Text = ""; _txtWifiGateway.Text = ""; _txtWifiSubnet.Text = ""; _txtWifiDns.Text = ""; _chkWifiDhcp.Checked = true;
        _selectedDevice = null;
    }

    // ─── Save & Reboot ─────────────────────────────────────────

    private async Task SaveAndReboot()
    {
        if (_selectedDevice == null || !_connected) { MessageBox.Show("먼저 기기를 연결하세요.", "알림"); return; }

        string mode = _cboNetMode.SelectedItem?.ToString() ?? "ethernet";
        string activeIp = mode == "wifi" ? _txtWifiIp.Text : _txtEthIp.Text;

        var result = MessageBox.Show($"{_txtDeviceName.Text}에 설정을 저장하고 재부팅하시겠습니까?\n모드: {mode}", "확인", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (result != DialogResult.Yes) return;

        _grid.Rows.Clear(); _devices.Clear(); _settingsPanel.Enabled = false; _connected = false;
        SetStatus("설정 저장 중...", true);

        var config = _configService.BuildConfigForIPSetup(
            _txtDeviceId.Text, _txtDeviceName.Text, mode,
            _chkEthDhcp.Checked, _txtEthIp.Text, _txtEthGateway.Text, _txtEthSubnet.Text, _txtEthDns.Text,
            _txtWifiSsid.Text, _txtWifiPassword.Text, _chkWifiDhcp.Checked,
            _txtWifiIp.Text, _txtWifiGateway.Text, _txtWifiSubnet.Text, _txtWifiDns.Text);

        // ETH 관리 IP로 설정 전송
        var mgmtDevice = new DeviceInfo { DeviceId = _selectedDevice.DeviceId, IP = _selectedDevice.ManagementIP, WebPort = 5050 };
        bool saved = await _discovery.SendConfigAsync(mgmtDevice, config);
        if (!saved) { SetStatus("설정 저장 실패.", false); return; }

        SetStatus("재부팅 중...", true);
        await _discovery.SendRebootAsync(mgmtDevice);

        if (!string.IsNullOrEmpty(activeIp)) _discovery.AddUnicastTarget(activeIp);
        CleanupTempIP();

        for (int i = 20; i > 0; i--) { SetStatus($"재부팅 대기 ({i}초)...", true); await Task.Delay(1000); }

        using (var ping = new System.Net.NetworkInformation.Ping())
        { for (int i = 0; i < 5; i++) { try { var r = await ping.SendPingAsync(activeIp, 1000); if (r.Status == System.Net.NetworkInformation.IPStatus.Success) break; } catch { } await Task.Delay(500); } }

        string targetId = _txtDeviceId.Text;
        DeviceInfo? found = null;
        for (int attempt = 1; attempt <= 3; attempt++)
        {
            SetStatus($"기기 검색 중... ({attempt}/3)", true);
            var devices = await _discovery.DiscoverDevicesAsync();
            found = devices.FirstOrDefault(d => d.DeviceId == targetId);
            if (found != null) { _devices = devices; ShowDevices(); break; }
            await Task.Delay(2000);
        }

        if (found != null)
        {
            SetStatus($"{found.IP} 재부팅 성공!", false);
            MessageBox.Show($"기기가 {found.IP}에서 동작 중입니다.\n\n웹 UI에서 MQTT 등 상세 설정을 진행하세요:\nhttp://{found.IP}:5050", "성공");
        }
        else SetStatus("재부팅 후 기기를 찾을 수 없습니다.", false);
        ClearFields();
    }

    // ─── Test / Web UI ─────────────────────────────────────────

    private async Task TestConnection()
    {
        if (_selectedDevice == null) { MessageBox.Show("기기를 연결하세요.", "알림"); return; }
        var mgmtIP = _selectedDevice.ManagementIP;
        SetStatus($"{mgmtIP} 연결 테스트...", true);
        var mgmtDevice = new DeviceInfo { DeviceId = _selectedDevice.DeviceId, IP = mgmtIP, WebPort = 5050 };
        var r = await _discovery.GetConfigViaUDPAsync(mgmtDevice);
        bool ok = r != null;
        SetStatus(ok ? "연결 성공!" : "연결 실패.", false);
        MessageBox.Show(ok ? $"{mgmtIP} 통신 정상!\n\n웹 UI: http://{_selectedDevice.IP}:5050" : $"{mgmtIP} 연결 불가", ok ? "성공" : "실패");
    }

    private void OpenWebUI()
    {
        if (_selectedDevice == null) return;
        try { System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo { FileName = $"http://{_selectedDevice.IP}:5050", UseShellExecute = true }); } catch { }
    }

    // ─── Helpers ────────────────────────────────────────────────

    private void SetStatus(string text, bool showProgress)
    {
        if (InvokeRequired) { Invoke(() => SetStatus(text, showProgress)); return; }
        _lblStatus.Text = text; _progressBar.Visible = showProgress;
    }

    private static bool ValidateIP(string ip) => IPAddress.TryParse(ip, out _);

    private static string GetPrimaryIP()
    {
        foreach (var nic in System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != System.Net.NetworkInformation.OperationalStatus.Up) continue;
            if (nic.NetworkInterfaceType == System.Net.NetworkInformation.NetworkInterfaceType.Loopback) continue;
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
                if (addr.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork) return addr.Address.ToString();
        }
        return "";
    }

    private static bool IsSameSubnet(string ip1, string ip2)
    {
        if (!IPAddress.TryParse(ip1, out var a) || !IPAddress.TryParse(ip2, out var b)) return false;
        var b1 = a.GetAddressBytes(); var b2 = b.GetAddressBytes();
        return b1[0] == b2[0] && b1[1] == b2[1] && b1[2] == b2[2];
    }

    private static void EnsureFirewallRule()
    {
        try
        {
            var check = new System.Diagnostics.ProcessStartInfo("netsh", "advfirewall firewall show rule name=\"RemoteDeck UDP\"") { CreateNoWindow = true, UseShellExecute = false, RedirectStandardOutput = true };
            var p = System.Diagnostics.Process.Start(check); p?.WaitForExit(3000); if (p?.ExitCode == 0) return;
            var add = new System.Diagnostics.ProcessStartInfo("netsh", $"advfirewall firewall add rule name=\"RemoteDeck UDP\" dir=in action=allow protocol=UDP program=\"{Environment.ProcessPath}\"") { CreateNoWindow = true, UseShellExecute = false };
            System.Diagnostics.Process.Start(add)?.WaitForExit(3000);
        } catch { }
    }
}
