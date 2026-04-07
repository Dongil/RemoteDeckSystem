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

    private TextBox _txtDeviceId = null!, _txtEthIp = null!, _txtEthGateway = null!,
                    _txtEthSubnet = null!, _txtEthDns = null!;
    private CheckBox _chkDhcp = null!;
    private TextBox _txtWifiSsid = null!, _txtWifiPassword = null!;
    private TextBox _txtMqttBroker = null!, _txtMqttPort = null!,
                    _txtMqttUser = null!, _txtMqttPassword = null!;

    private Button _btnConnect = null!, _btnSave = null!, _btnTest = null!, _btnWeb = null!;
    private Label _lblStatus = null!;
    private ProgressBar _progressBar = null!;
    private Panel _settingsPanel = null!;

    public MainForm()
    {
        Text = "RemoteDeck IP 설정 도구 v1.0";
        Size = new Size(620, 860);
        StartPosition = FormStartPosition.CenterScreen;
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;

        BuildUI();
        FormClosing += (_, _) => CleanupTempIP();
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
            Location = new Point(10, y),
            Size = new Size(580, 120),
            ReadOnly = true,
            SelectionMode = DataGridViewSelectionMode.FullRowSelect,
            MultiSelect = false,
            AllowUserToAddRows = false,
            AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
        };
        _grid.Columns.Add("DeviceId", "장치 ID");
        _grid.Columns.Add("IP", "IP 주소");
        _grid.Columns.Add("MAC", "MAC 주소");
        _grid.Columns.Add("FW", "펌웨어");
        Controls.Add(_grid);
        y += 125;

        // Manual IP + Connect
        _btnConnect = new Button { Text = "장치 연결", Location = new Point(10, y), Size = new Size(100, 30), Enabled = false };
        _btnConnect.Click += async (_, _) => await ConnectToDevice();
        Controls.Add(_btnConnect);

        Controls.Add(new Label { Text = "IP 직접입력:", Location = new Point(120, y + 5), AutoSize = true });
        var txtManualIp = new TextBox { Location = new Point(195, y + 2), Size = new Size(140, 23), PlaceholderText = "192.168.x.x" };
        Controls.Add(txtManualIp);

        var btnManualConnect = new Button { Text = "직접 연결", Location = new Point(340, y), Size = new Size(90, 30) };
        btnManualConnect.Click += async (_, _) =>
        {
            var ip = txtManualIp.Text.Trim();
            if (!ValidateIP(ip)) { MessageBox.Show("올바른 IP를 입력하세요.", "알림"); return; }

            // Check if we need a temp IP for this subnet
            if (!IsSameSubnet(ip, GetPrimaryIP()))
            {
                SetStatus($"{ip} 서브넷 접근을 위해 임시 IP 추가 중...", true);
                CleanupTempIP();

                // Generate temp IP on same subnet: x.x.x.100
                var parts = ip.Split('.');
                var tempIp = $"{parts[0]}.{parts[1]}.{parts[2]}.100";

                var nics = NetworkHelper.GetPhysicalNICs();
                if (nics.Count > 0)
                {
                    var nicName = nics[0].Name;
                    NetworkHelper.AddTempIPCustom(nicName, tempIp);
                    _tempIPNic = nicName;

                    SetStatus("임시 IP 적용 및 ARP 확인 중...", true);
                    await Task.Delay(3000);

                    // ARP warmup
                    using var ping = new System.Net.NetworkInformation.Ping();
                    for (int i = 0; i < 5; i++)
                    {
                        try
                        {
                            var reply = await ping.SendPingAsync(ip, 1000);
                            if (reply.Status == System.Net.NetworkInformation.IPStatus.Success) break;
                        }
                        catch { }
                        await Task.Delay(500);
                    }
                    await Task.Delay(1000);
                }
            }

            _selectedDevice = new DeviceInfo { DeviceId = "unknown", IP = ip, WebPort = 5050 };
            _grid.Rows.Clear();
            _grid.Rows.Add("?", ip, "?", "?");
            _devices = new List<DeviceInfo> { _selectedDevice };
            await ConnectToDevice();
        };
        Controls.Add(btnManualConnect);

        _grid.SelectionChanged += (_, _) =>
        {
            _btnConnect.Enabled = _grid.SelectedRows.Count > 0 && _grid.SelectedRows[0].Index < _devices.Count;
        };
        y += 40;

        _settingsPanel = new Panel
        {
            Location = new Point(0, y),
            Size = new Size(610, 460),
            Enabled = false
        };
        Controls.Add(_settingsPanel);

        int py = 0;

        _settingsPanel.Controls.Add(new Label { Text = "── 네트워크 설정 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "장치 ID:", ref _txtDeviceId, ref py, "");
        AddLabeledField(_settingsPanel, "IP 주소:", ref _txtEthIp, ref py, "");
        AddLabeledField(_settingsPanel, "서브넷 마스크:", ref _txtEthSubnet, ref py, "");
        AddLabeledField(_settingsPanel, "게이트웨이:", ref _txtEthGateway, ref py, "");
        AddLabeledField(_settingsPanel, "DNS:", ref _txtEthDns, ref py, "");

        _chkDhcp = new CheckBox { Text = "DHCP 사용", Location = new Point(120, py), AutoSize = true };
        _chkDhcp.CheckedChanged += (_, _) =>
        {
            bool on = _chkDhcp.Checked;
            _txtEthIp.Enabled = !on;
            _txtEthSubnet.Enabled = !on;
            _txtEthGateway.Enabled = !on;
            _txtEthDns.Enabled = !on;
        };
        _settingsPanel.Controls.Add(_chkDhcp);
        py += 30;

        _settingsPanel.Controls.Add(new Label { Text = "── WiFi 설정 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "WiFi SSID:", ref _txtWifiSsid, ref py, "");
        AddLabeledField(_settingsPanel, "WiFi 비밀번호:", ref _txtWifiPassword, ref py, "");
        py += 5;

        _settingsPanel.Controls.Add(new Label { Text = "── MQTT 설정 ──", Location = new Point(10, py), AutoSize = true, ForeColor = Color.DarkCyan });
        py += 22;
        AddLabeledField(_settingsPanel, "브로커:", ref _txtMqttBroker, ref py, "");
        AddLabeledField(_settingsPanel, "포트:", ref _txtMqttPort, ref py, "");
        AddLabeledField(_settingsPanel, "사용자:", ref _txtMqttUser, ref py, "");
        AddLabeledField(_settingsPanel, "비밀번호:", ref _txtMqttPassword, ref py, "");
        py += 10;

        _btnSave = new Button { Text = "저장 && 재부팅", Location = new Point(10, py), Size = new Size(150, 35) };
        _btnSave.Click += async (_, _) => await SaveAndReboot();
        _settingsPanel.Controls.Add(_btnSave);

        _btnTest = new Button { Text = "연결 테스트", Location = new Point(170, py), Size = new Size(130, 35) };
        _btnTest.Click += async (_, _) => await TestConnection();
        _settingsPanel.Controls.Add(_btnTest);

        _btnWeb = new Button { Text = "웹 UI 열기", Location = new Point(310, py), Size = new Size(120, 35) };
        _btnWeb.Click += (_, _) => OpenWebUI();
        _settingsPanel.Controls.Add(_btnWeb);

        y += _settingsPanel.Height + 5;

        _progressBar = new ProgressBar { Location = new Point(10, y), Size = new Size(580, 20), Style = ProgressBarStyle.Marquee, Visible = false };
        Controls.Add(_progressBar);
        y += 25;

        _lblStatus = new Label { Text = "'기기 검색' 버튼을 눌러 시작하세요.", Location = new Point(10, y), Size = new Size(580, 40), ForeColor = Color.Gray };
        Controls.Add(_lblStatus);
    }

    private void AddLabeledField(Control parent, string label, ref TextBox textBox, ref int y, string defaultValue)
    {
        parent.Controls.Add(new Label { Text = label, Location = new Point(10, y + 3), Size = new Size(105, 20), TextAlign = ContentAlignment.MiddleRight });
        textBox = new TextBox { Location = new Point(120, y), Size = new Size(250, 23), Text = defaultValue };
        parent.Controls.Add(textBox);
        y += 28;
    }

    // ─── Smart Discover: normal search → if empty, offer NIC selection ───

    private async Task SmartDiscover()
    {
        SetStatus("기기 검색 중...", true);
        _grid.Rows.Clear();
        _connected = false;
        _settingsPanel.Enabled = false;
        ClearFields();

        // Add firewall rule for UDP 5051 (first run only, silently)
        EnsureFirewallRule();

        // Step 1: Normal discovery (broadcast + unicast to known IPs)
        _devices = await _discovery.DiscoverDevicesAsync();

        if (_devices.Count > 0)
        {
            ShowDevices();
            SetStatus($"{_devices.Count}대의 기기를 발견했습니다.", false);
            return;
        }

        // Step 2: Not found
        bool hasMatchingSubnet = NetworkHelper.HasSubnet192168_1();

        string message;
        if (hasMatchingSubnet)
        {
            message = "같은 서브넷(192.168.1.x)에서 기기를 찾을 수 없습니다.\n\n" +
                      "확인 사항:\n" +
                      "  - 기기 전원이 켜져 있는지\n" +
                      "  - 이더넷 케이블이 연결되어 있는지\n" +
                      "  - Windows 방화벽에서 UDP 5051이 차단되지 않는지\n\n" +
                      "네트워크 어댑터에 임시 IP를 추가하여 재검색하시겠습니까?";
        }
        else
        {
            message = "기기를 찾을 수 없습니다.\n\n" +
                      "PC와 기기가 다른 서브넷에 있을 수 있습니다.\n" +
                      "기기의 초기 IP(192.168.1.200)에 접근하기 위해\n" +
                      "네트워크 어댑터에 임시 IP를 추가하시겠습니까?";
        }

        SetStatus("", false);
        var answer = MessageBox.Show(message,
            "기기 검색", MessageBoxButtons.YesNo, MessageBoxIcon.Question);

        if (answer != DialogResult.Yes)
        {
            SetStatus("검색 취소됨.", false);
            return;
        }

        // Step 4: Show NIC selection
        var nics = NetworkHelper.GetPhysicalNICs();
        if (nics.Count == 0)
        {
            MessageBox.Show("활성화된 네트워크 어댑터가 없습니다.", "오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        string? selectedNicName = null;

        if (nics.Count == 1)
        {
            // Only one NIC - use it automatically
            selectedNicName = nics[0].Name;
        }
        else
        {
            // Multiple NICs - let user choose
            var selectForm = new Form
            {
                Text = "네트워크 어댑터 선택",
                Size = new Size(500, 250),
                StartPosition = FormStartPosition.CenterParent,
                FormBorderStyle = FormBorderStyle.FixedDialog,
                MaximizeBox = false,
                MinimizeBox = false
            };

            selectForm.Controls.Add(new Label
            {
                Text = "기기가 연결된 네트워크 어댑터를 선택하세요:",
                Location = new Point(15, 10),
                Size = new Size(460, 25)
            });

            var listBox = new ListBox { Location = new Point(15, 40), Size = new Size(450, 120) };
            foreach (var n in nics)
                listBox.Items.Add($"{n.Name}  ({n.Description})  [{string.Join(", ", n.IPs)}]");
            listBox.SelectedIndex = 0;
            selectForm.Controls.Add(listBox);

            var btnOk = new Button { Text = "확인", Location = new Point(300, 170), Size = new Size(80, 30), DialogResult = DialogResult.OK };
            var btnCancel = new Button { Text = "취소", Location = new Point(385, 170), Size = new Size(80, 30), DialogResult = DialogResult.Cancel };
            selectForm.Controls.Add(btnOk);
            selectForm.Controls.Add(btnCancel);
            selectForm.AcceptButton = btnOk;
            selectForm.CancelButton = btnCancel;

            if (selectForm.ShowDialog(this) != DialogResult.OK || listBox.SelectedIndex < 0)
                return;

            selectedNicName = nics[listBox.SelectedIndex].Name;
        }

        // Step 5: Add temporary IP
        SetStatus($"'{selectedNicName}'에 임시 IP (192.168.1.100) 추가 중...", true);

        var (added, addError) = NetworkHelper.AddTempIP(selectedNicName);
        if (!added)
        {
            SetStatus("임시 IP 추가 실패.", false);
            MessageBox.Show($"임시 IP를 추가할 수 없습니다.\n\n{addError}\n\n" +
                $"관리자 권한: {(NetworkHelper.IsRunningAsAdmin() ? "예" : "아니오")}\n" +
                "관리자 권한이 아니면 우클릭 → 관리자 권한으로 실행하세요.\n\n" +
                "상세 로그: network_log.txt",
                "오류", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        _tempIPNic = selectedNicName;

        // Wait for IP to become active
        SetStatus("임시 IP 적용 대기 중...", true);
        await Task.Delay(3000);

        // ARP warmup: ping the default device IP to establish bidirectional ARP
        SetStatus("기기 연결 확인 중 (ARP)...", true);
        using (var ping = new System.Net.NetworkInformation.Ping())
        {
            for (int i = 0; i < 5; i++)
            {
                try
                {
                    var reply = await ping.SendPingAsync("192.168.1.200", 1000);
                    if (reply.Status == System.Net.NetworkInformation.IPStatus.Success)
                    {
                        SetStatus($"기기 응답 확인 (ping {reply.RoundtripTime}ms)", true);
                        break;
                    }
                }
                catch { }
                await Task.Delay(500);
            }
        }
        await Task.Delay(1000);

        // Step 6: Search again with retry
        for (int retry = 1; retry <= 3; retry++)
        {
            SetStatus($"기기 재검색 중... (시도 {retry}/3)", true);
            _devices = await _discovery.DiscoverDevicesAsync();
            if (_devices.Count > 0) break;
            await Task.Delay(2000);
        }

        if (_devices.Count > 0)
        {
            ShowDevices();
            SetStatus($"{_devices.Count}대 발견. 장치 연결 후 현장 네트워크에 맞게 IP를 변경하세요. (임시 IP 사용 중)", false);
        }
        else
        {
            SetStatus("기기를 찾을 수 없습니다. 기기 전원과 케이블 연결을 확인하세요.", false);
        }
    }

    private void ShowDevices()
    {
        _grid.Rows.Clear();
        foreach (var d in _devices)
        {
            _grid.Rows.Add(d.DeviceId, d.IP, d.MAC, d.FirmwareVersion);
        }
        _btnConnect.Enabled = _devices.Count > 0;
    }

    private void CleanupTempIP()
    {
        if (_tempIPNic != null)
        {
            NetworkHelper.RemoveTempIP(_tempIPNic);
            _tempIPNic = null;
        }
    }

    // ─── Connect ───────────────────────────────────────────────

    private async Task ConnectToDevice()
    {
        if (_grid.SelectedRows.Count == 0) return;

        int idx = _grid.SelectedRows[0].Index;
        if (idx < 0 || idx >= _devices.Count) return;

        _selectedDevice = _devices[idx];
        SetStatus($"{_selectedDevice.IP}에서 설정을 불러오는 중...", true);

        // Try UDP first (works across subnets), fallback to HTTP
        var config = await _discovery.GetConfigViaUDPAsync(_selectedDevice);
        if (config == null)
        {
            config = await _configService.GetConfigAsync(_selectedDevice.IP, _selectedDevice.WebPort);
        }
        if (config != null)
        {
            _txtDeviceId.Text = config.DeviceId;
            _chkDhcp.Checked = config.Network.Ethernet.Dhcp;
            _txtEthIp.Text = config.Network.Ethernet.IP;
            _txtEthGateway.Text = config.Network.Ethernet.Gateway;
            _txtEthSubnet.Text = config.Network.Ethernet.Subnet;
            _txtEthDns.Text = config.Network.Ethernet.Dns1;
            _txtWifiSsid.Text = config.Network.Wifi.Ssid;
            _txtWifiPassword.Text = config.Network.Wifi.Password;
            _txtMqttBroker.Text = config.Mqtt.Broker;
            _txtMqttPort.Text = config.Mqtt.Port.ToString();
            _txtMqttUser.Text = config.Mqtt.User;
            _txtMqttPassword.Text = config.Mqtt.Password;

            // Update device info from loaded config
            _selectedDevice.DeviceId = config.DeviceId;
            _connected = true;
            _settingsPanel.Enabled = true;
            SetStatus($"{_selectedDevice.DeviceId} ({_selectedDevice.IP})에 연결되었습니다.", false);
        }
        else
        {
            SetStatus($"{_selectedDevice.IP}에서 설정을 불러올 수 없습니다.", false);
            MessageBox.Show($"{_selectedDevice.IP}에 연결할 수 없습니다.",
                "연결 실패", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void ClearFields()
    {
        _txtDeviceId.Text = "";
        _txtEthIp.Text = "";
        _txtEthSubnet.Text = "";
        _txtEthGateway.Text = "";
        _txtEthDns.Text = "";
        _chkDhcp.Checked = false;
        _txtWifiSsid.Text = "";
        _txtWifiPassword.Text = "";
        _txtMqttBroker.Text = "";
        _txtMqttPort.Text = "";
        _txtMqttUser.Text = "";
        _txtMqttPassword.Text = "";
        _selectedDevice = null;
    }

    // ─── Save & Reboot ─────────────────────────────────────────

    private async Task SaveAndReboot()
    {
        if (_selectedDevice == null || !_connected)
        {
            MessageBox.Show("먼저 기기를 검색하고 연결하세요.", "알림", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        if (!ValidateIP(_txtEthIp.Text) && !_chkDhcp.Checked)
        {
            MessageBox.Show("IP 주소 형식이 올바르지 않습니다.", "입력 오류", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return;
        }

        var result = MessageBox.Show(
            $"{_selectedDevice.DeviceId} ({_selectedDevice.IP})에 설정을 저장하고 재부팅하시겠습니까?",
            "확인", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (result != DialogResult.Yes) return;

        _grid.Rows.Clear();
        _devices.Clear();
        _settingsPanel.Enabled = false;
        _connected = false;

        SetStatus("설정 저장 중...", true);

        int.TryParse(_txtMqttPort.Text, out int mqttPort);
        var config = _configService.BuildConfigFromForm(
            _txtDeviceId.Text, "ethernet",
            _chkDhcp.Checked, _txtEthIp.Text, _txtEthGateway.Text, _txtEthSubnet.Text, _txtEthDns.Text,
            _txtWifiSsid.Text, _txtWifiPassword.Text, true,
            _txtMqttBroker.Text, mqttPort, _txtMqttUser.Text, _txtMqttPassword.Text);

        bool saved = await _discovery.SendConfigAsync(_selectedDevice, config);
        if (!saved)
        {
            SetStatus("설정 저장에 실패했습니다.", false);
            return;
        }

        SetStatus("설정 저장 완료. 기기 재부팅 중...", true);
        await _discovery.SendRebootAsync(_selectedDevice);

        // Add new IP to unicast targets for post-reboot search
        _discovery.AddUnicastTarget(_txtEthIp.Text);

        // Remember the new IP for post-reboot search
        string newIp = _txtEthIp.Text;
        bool newIpSameSubnet = IsSameSubnet(newIp, GetPrimaryIP());

        CleanupTempIP();

        for (int i = 20; i > 0; i--)
        {
            SetStatus($"기기 재부팅 대기 중 ({i}초)...", true);
            await Task.Delay(1000);
        }

        // If new IP is on a different subnet, re-add temp IP and do ARP warmup
        if (!newIpSameSubnet)
        {
            SetStatus("다른 서브넷 기기 접근을 위해 임시 IP 설정 중...", true);
            var nics = NetworkHelper.GetPhysicalNICs();
            if (nics.Count > 0)
            {
                var nicName = nics[0].Name;
                NetworkHelper.AddTempIP(nicName);
                _tempIPNic = nicName;
                await Task.Delay(3000);
            }
        }

        // ARP warmup with ping to new IP
        SetStatus("기기 연결 확인 중...", true);
        using (var ping = new System.Net.NetworkInformation.Ping())
        {
            for (int i = 0; i < 5; i++)
            {
                try
                {
                    var reply = await ping.SendPingAsync(newIp, 1000);
                    if (reply.Status == System.Net.NetworkInformation.IPStatus.Success) break;
                }
                catch { }
                await Task.Delay(500);
            }
        }
        await Task.Delay(1000);

        string targetId = _txtDeviceId.Text;
        DeviceInfo? found = null;

        for (int attempt = 1; attempt <= 3; attempt++)
        {
            SetStatus($"재부팅된 기기 검색 중... (시도 {attempt}/3)", true);
            var devices = await _discovery.DiscoverDevicesAsync();
            found = devices.FirstOrDefault(d => d.DeviceId == targetId);

            if (found != null)
            {
                _devices = devices;
                ShowDevices();
                break;
            }

            await Task.Delay(2000);
        }

        if (found != null)
        {
            SetStatus($"{found.IP}에서 기기를 발견했습니다. 저장 및 재부팅 성공!", false);
            MessageBox.Show($"'{found.DeviceId}' 기기가 {found.IP}에서 동작 중입니다.", "성공",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
        else
        {
            SetStatus("재부팅 후 기기를 찾을 수 없습니다. '기기 검색'을 다시 시도하세요.", false);
            MessageBox.Show("재부팅 후 기기를 자동으로 찾지 못했습니다.\n잠시 후 '기기 검색'을 눌러 다시 검색하세요.",
                "알림", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        ClearFields();
    }

    // ─── Test / Web UI ─────────────────────────────────────────

    private async Task TestConnection()
    {
        if (_selectedDevice == null)
        {
            MessageBox.Show("기기를 선택하고 연결하세요.", "알림", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }

        SetStatus($"{_selectedDevice.IP} UDP 연결 테스트 중...", true);

        // Use UDP DISCOVER for connection test (works on both Ethernet2 and WiFi)
        var testResult = await _discovery.GetConfigViaUDPAsync(_selectedDevice);
        bool ok = testResult != null;

        SetStatus(ok ? $"{_selectedDevice.IP} 연결 성공!" : $"{_selectedDevice.IP} 연결 실패.", false);
        MessageBox.Show(
            ok ? $"{_selectedDevice.IP} UDP 통신 정상!\n\n웹 UI는 WiFi AP에 연결하여 접속하세요.\nSSID: RemoteDeck_{_selectedDevice.DeviceId}\nPW: remotedeck\nURL: http://192.168.4.1:5050"
               : $"{_selectedDevice.IP}에 연결할 수 없습니다.",
            ok ? "성공" : "실패", MessageBoxButtons.OK,
            ok ? MessageBoxIcon.Information : MessageBoxIcon.Error);
    }

    private void OpenWebUI()
    {
        // Web UI runs on WiFi AP (192.168.4.1), not on Ethernet
        var result = MessageBox.Show(
            "웹 UI는 기기의 WiFi AP에 연결해야 접속할 수 있습니다.\n\n" +
            "1. WiFi에서 'RemoteDeck_" + (_selectedDevice?.DeviceId ?? "node_1") + "' 연결\n" +
            "2. 비밀번호: remotedeck\n" +
            "3. 브라우저에서 http://192.168.4.1:5050 접속\n\n" +
            "브라우저를 여시겠습니까?",
            "웹 UI 접속 안내", MessageBoxButtons.YesNo, MessageBoxIcon.Information);

        if (result != DialogResult.Yes) return;

        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = "http://192.168.4.1:5050",
                UseShellExecute = true
            });
        }
        catch { }
    }

    private void SetStatus(string text, bool showProgress)
    {
        if (InvokeRequired) { Invoke(() => SetStatus(text, showProgress)); return; }
        _lblStatus.Text = text;
        _progressBar.Visible = showProgress;
    }

    private static bool ValidateIP(string ip)
    {
        return IPAddress.TryParse(ip, out _);
    }

    private static string GetPrimaryIP()
    {
        foreach (var nic in System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != System.Net.NetworkInformation.OperationalStatus.Up) continue;
            if (nic.NetworkInterfaceType == System.Net.NetworkInformation.NetworkInterfaceType.Loopback) continue;
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
            {
                if (addr.Address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
                    return addr.Address.ToString();
            }
        }
        return "";
    }

    private static bool IsSameSubnet(string ip1, string ip2)
    {
        if (!IPAddress.TryParse(ip1, out var a) || !IPAddress.TryParse(ip2, out var b)) return false;
        var bytes1 = a.GetAddressBytes();
        var bytes2 = b.GetAddressBytes();
        // Compare first 3 octets (assumes /24)
        return bytes1[0] == bytes2[0] && bytes1[1] == bytes2[1] && bytes1[2] == bytes2[2];
    }

    private static void EnsureFirewallRule()
    {
        try
        {
            var exePath = Environment.ProcessPath ?? "";
            var check = new System.Diagnostics.ProcessStartInfo("netsh",
                "advfirewall firewall show rule name=\"RemoteDeck UDP\"")
            {
                CreateNoWindow = true, UseShellExecute = false,
                RedirectStandardOutput = true
            };
            var p = System.Diagnostics.Process.Start(check);
            p?.WaitForExit(3000);
            if (p?.ExitCode == 0) return;

            // Allow ALL UDP for this program (covers broadcast, unicast, any port)
            var add = new System.Diagnostics.ProcessStartInfo("netsh",
                $"advfirewall firewall add rule name=\"RemoteDeck UDP\" " +
                $"dir=in action=allow protocol=UDP program=\"{exePath}\"")
            {
                CreateNoWindow = true, UseShellExecute = false
            };
            var p2 = System.Diagnostics.Process.Start(add);
            p2?.WaitForExit(3000);
        }
        catch { }
    }
}
