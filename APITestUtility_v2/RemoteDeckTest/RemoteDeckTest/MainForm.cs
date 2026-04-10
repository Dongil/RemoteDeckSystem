using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using MQTTnet;
using MQTTnet.Client;
using System.IO.Ports;

namespace RemoteDeckTest;

public partial class MainForm : Form
{
    // Connection
    private HttpClient? _http;
    private IMqttClient? _mqtt;
    private ClientWebSocket? _ws;
    private SerialPort? _serial;
    private CancellationTokenSource? _wsCts;

    public MainForm()
    {
        InitializeComponent();
    }

    // ─── HTTP API ───────────────────────────────────────

    private async Task<string?> HttpSend(string method, string path, string? body = null)
    {
        try
        {
            var ip = txtIP.Text.Trim();
            var port = txtWebPort.Text.Trim();
            var url = $"http://{ip}:{port}{path}";

            _http ??= new HttpClient();
            _http.DefaultRequestHeaders.Authorization = new System.Net.Http.Headers.AuthenticationHeaderValue(
                "Basic", Convert.ToBase64String(Encoding.UTF8.GetBytes($"{txtUser.Text}:{txtPass.Text}")));

            HttpResponseMessage resp;
            if (method == "POST")
                resp = await _http.PostAsync(url, string.IsNullOrEmpty(body) ? null : new StringContent(body, Encoding.UTF8, "application/json"));
            else
                resp = await _http.GetAsync(url);

            var result = await resp.Content.ReadAsStringAsync();
            Log($"[HTTP] {method} {path} → {(int)resp.StatusCode}: {result}");
            return result;
        }
        catch (Exception ex)
        {
            Log($"[HTTP] Error: {ex.Message}");
            return null;
        }
    }

    // ─── MQTT ───────────────────────────────────────────

    private async Task MqttConnect()
    {
        try
        {
            var factory = new MqttFactory();
            _mqtt = factory.CreateMqttClient();

            var optBuilder = new MqttClientOptionsBuilder()
                .WithTcpServer(txtMqttBroker.Text.Trim(), int.Parse(txtMqttPort.Text.Trim()))
                .WithClientId($"RDTest-{Guid.NewGuid().ToString()[..6]}");

            if (!string.IsNullOrEmpty(txtMqttUser.Text))
                optBuilder.WithCredentials(txtMqttUser.Text, txtMqttPass.Text);

            _mqtt.ApplicationMessageReceivedAsync += e =>
            {
                var payload = Encoding.UTF8.GetString(e.ApplicationMessage.PayloadSegment);
                Invoke(() =>
                {
                    Log($"[MQTT] ← [{e.ApplicationMessage.Topic}] {payload}");
                    TryParseStatus(payload);
                });
                return Task.CompletedTask;
            };

            _mqtt.ConnectedAsync += async e =>
            {
                var subTopic = txtMqttSubTopic.Text.Trim();
                await _mqtt.SubscribeAsync(subTopic);
                Invoke(() => Log($"[MQTT] Connected, subscribed: {subTopic}"));
            };

            _mqtt.DisconnectedAsync += e =>
            {
                Invoke(() => Log("[MQTT] Disconnected"));
                return Task.CompletedTask;
            };

            await _mqtt.ConnectAsync(optBuilder.Build());
            btnMqttConnect.Text = "Disconnect";
        }
        catch (Exception ex)
        {
            Log($"[MQTT] Error: {ex.Message}");
        }
    }

    private async Task MqttDisconnect()
    {
        if (_mqtt?.IsConnected == true)
            await _mqtt.DisconnectAsync();
        _mqtt?.Dispose();
        _mqtt = null;
        btnMqttConnect.Text = "Connect";
        Log("[MQTT] Disconnected");
    }

    private async Task MqttPublish(string json)
    {
        if (_mqtt?.IsConnected != true) { Log("[MQTT] Not connected"); return; }
        var topic = txtMqttPubTopic.Text.Trim();
        await _mqtt.PublishStringAsync(topic, json);
        Log($"[MQTT] → [{topic}] {json}");
    }

    // ─── WebSocket ──────────────────────────────────────

    private async Task WsConnect()
    {
        try
        {
            _ws = new ClientWebSocket();
            var ip = txtIP.Text.Trim();
            var port = txtWebPort.Text.Trim();
            var cred = Convert.ToBase64String(Encoding.UTF8.GetBytes($"{txtUser.Text}:{txtPass.Text}"));
            _ws.Options.SetRequestHeader("Authorization", $"Basic {cred}");
            _wsCts = new CancellationTokenSource();
            await _ws.ConnectAsync(new Uri($"ws://{ip}:{port}/ws"), _wsCts.Token);
            Log("[WS] Connected");
            btnWsConnect.Text = "Disconnect";
            _ = WsReceiveLoop();
        }
        catch (Exception ex)
        {
            Log($"[WS] Error: {ex.Message}");
        }
    }

    private async Task WsReceiveLoop()
    {
        var buf = new byte[4096];
        try
        {
            while (_ws?.State == WebSocketState.Open && _wsCts?.IsCancellationRequested == false)
            {
                var result = await _ws.ReceiveAsync(buf, _wsCts.Token);
                if (result.MessageType == WebSocketMessageType.Text)
                {
                    var msg = Encoding.UTF8.GetString(buf, 0, result.Count);
                    Invoke(() =>
                    {
                        Log($"[WS] ← {msg}");
                        TryParseWsStatus(msg);
                    });
                }
                else if (result.MessageType == WebSocketMessageType.Close)
                {
                    break;
                }
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { Invoke(() => Log($"[WS] Recv error: {ex.Message}")); }
        Invoke(() => { btnWsConnect.Text = "Connect"; Log("[WS] Disconnected"); });
    }

    private async Task WsDisconnect()
    {
        _wsCts?.Cancel();
        if (_ws?.State == WebSocketState.Open)
            await _ws.CloseAsync(WebSocketCloseStatus.NormalClosure, "", CancellationToken.None);
        _ws?.Dispose();
        _ws = null;
        btnWsConnect.Text = "Connect";
    }

    // ─── Serial (RS485) ─────────────────────────────────

    private void SerialConnect()
    {
        try
        {
            _serial = new SerialPort(cboSerialPort.Text, int.Parse(txtBaud.Text), Parity.None, 8, StopBits.One);
            _serial.DataReceived += (s, e) =>
            {
                var line = _serial.ReadLine().Trim();
                Invoke(() =>
                {
                    Log($"[RS485] ← {line}");
                    TryParseStatus(line);
                });
            };
            _serial.Open();
            Log($"[RS485] Opened {cboSerialPort.Text} @ {txtBaud.Text}");
            btnSerialConnect.Text = "Disconnect";
        }
        catch (Exception ex)
        {
            Log($"[RS485] Error: {ex.Message}");
        }
    }

    private void SerialDisconnect()
    {
        _serial?.Close();
        _serial?.Dispose();
        _serial = null;
        btnSerialConnect.Text = "Connect";
        Log("[RS485] Closed");
    }

    private void SerialSend(string json)
    {
        if (_serial?.IsOpen != true) { Log("[RS485] Not open"); return; }
        _serial.WriteLine(json);
        Log($"[RS485] → {json}");
    }

    // ─── Command Senders (v2.2 format) ──────────────────

    private async Task SendCommand(string json, string? httpPath = null)
    {
        var iface = cboInterface.SelectedItem?.ToString() ?? "HTTP";
        switch (iface)
        {
            case "HTTP":
                await HttpSend("POST", httpPath ?? "/api/relay", json);
                break;
            case "MQTT":
                await MqttPublish(json);
                break;
            case "RS485":
                SerialSend(json);
                break;
        }
    }

    private async void btnRelay1On_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"relay\",\"relay\":1,\"state\":\"on\"}", "/api/relay");
    private async void btnRelay1Off_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"relay\",\"relay\":1,\"state\":\"off\"}", "/api/relay");
    private async void btnRelay1Pulse_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"pulse\",\"relay\":1}", "/api/relay");
    private async void btnRelay2On_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"relay\",\"relay\":2,\"state\":\"on\"}", "/api/relay");
    private async void btnRelay2Off_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"relay\",\"relay\":2,\"state\":\"off\"}", "/api/relay");
    private async void btnRelay2Pulse_Click(object? s, EventArgs e)
        => await SendCommand("{\"cmd\":\"pulse\",\"relay\":2}", "/api/relay");
    private async void btnGetStatus_Click(object? s, EventArgs e)
    {
        var iface = cboInterface.SelectedItem?.ToString() ?? "HTTP";
        if (iface == "HTTP")
        {
            var result = await HttpSend("GET", "/api/status");
            if (result != null) TryParseWsStatus(result);
        }
        else
            await SendCommand("{\"cmd\":\"status\"}");
    }
    private async void btnWOL_Click(object? s, EventArgs e)
        => await SendCommand("{\"mac\":\"\"}", "/api/wol");
    private async void btnReboot_Click(object? s, EventArgs e)
    {
        if (MessageBox.Show("장치를 재부팅하시겠습니까?", "확인", MessageBoxButtons.YesNo) == DialogResult.Yes)
            await SendCommand("", "/api/reboot");
    }

    private async void btnHttpStatus_Click(object? s, EventArgs e)
    {
        var result = await HttpSend("GET", "/api/status");
        if (result != null) TryParseWsStatus(result);
    }

    private async void btnHttpConfig_Click(object? s, EventArgs e)
        => await HttpSend("GET", "/api/config");

    // ─── Connection Buttons ─────────────────────────────

    private async void btnMqttConnect_Click(object? s, EventArgs e)
    {
        if (_mqtt?.IsConnected == true) await MqttDisconnect();
        else await MqttConnect();
    }

    private async void btnWsConnect_Click(object? s, EventArgs e)
    {
        if (_ws?.State == WebSocketState.Open) await WsDisconnect();
        else await WsConnect();
    }

    private void btnSerialConnect_Click(object? s, EventArgs e)
    {
        if (_serial?.IsOpen == true) SerialDisconnect();
        else SerialConnect();
    }

    private void btnRefreshPorts_Click(object? s, EventArgs e)
    {
        cboSerialPort.Items.Clear();
        foreach (var p in SerialPort.GetPortNames())
            cboSerialPort.Items.Add(p);
        if (cboSerialPort.Items.Count > 0) cboSerialPort.SelectedIndex = 0;
    }

    private void btnClearLog_Click(object? s, EventArgs e)
        => txtLog.Clear();

    // ─── Status Parsing ─────────────────────────────────

    private void TryParseStatus(string json)
    {
        try
        {
            var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            if (root.TryGetProperty("event", out var ev))
            {
                var evName = ev.GetString();
                if (evName == "relay" || evName == "full")
                {
                    if (root.TryGetProperty("relay1", out var r1)) UpdateIndicator(lblRelay1, r1.GetInt32() != 0);
                    if (root.TryGetProperty("relay2", out var r2)) UpdateIndicator(lblRelay2, r2.GetInt32() != 0);
                }
                if (evName == "pcled" || evName == "full")
                {
                    if (root.TryGetProperty("pc_on", out var pc)) UpdateIndicator(lblPCLed, pc.GetBoolean());
                }
                if (evName == "gpio" || evName == "full")
                {
                    if (root.TryGetProperty("gpio1", out var g1)) UpdateIndicator(lblGPIO1, g1.GetInt32() != 0);
                    if (root.TryGetProperty("gpio2", out var g2)) UpdateIndicator(lblGPIO2, g2.GetInt32() != 0);
                    if (root.TryGetProperty("gpio3", out var g3)) UpdateIndicator(lblGPIO3, g3.GetInt32() != 0);
                }
            }
        }
        catch { }
    }

    private void TryParseWsStatus(string json)
    {
        try
        {
            var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            // WebSocket status format (internal)
            if (root.TryGetProperty("pc_on", out var pc)) UpdateIndicator(lblPCLed, pc.GetBoolean());
            if (root.TryGetProperty("relay1", out var r1)) UpdateIndicator(lblRelay1, r1.GetBoolean());
            if (root.TryGetProperty("relay2", out var r2)) UpdateIndicator(lblRelay2, r2.GetBoolean());
            if (root.TryGetProperty("gpio", out var gpio) && gpio.ValueKind == JsonValueKind.Array)
            {
                var arr = gpio.EnumerateArray().ToArray();
                if (arr.Length >= 1) UpdateIndicator(lblGPIO1, arr[0].GetInt32() != 0);
                if (arr.Length >= 2) UpdateIndicator(lblGPIO2, arr[1].GetInt32() != 0);
                if (arr.Length >= 3) UpdateIndicator(lblGPIO3, arr[2].GetInt32() != 0);
            }

            // Also try v2.2 event format
            if (root.TryGetProperty("event", out _))
                TryParseStatus(json);
        }
        catch { }
    }

    private void UpdateIndicator(Label lbl, bool on)
    {
        lbl.BackColor = on ? Color.LimeGreen : Color.DarkRed;
        lbl.Text = on ? "ON" : "OFF";
    }

    // ─── Logging ────────────────────────────────────────

    private void Log(string msg)
    {
        var line = $"[{DateTime.Now:HH:mm:ss}] {msg}\r\n";
        if (txtLog.InvokeRequired)
            txtLog.Invoke(() => { txtLog.AppendText(line); });
        else
            txtLog.AppendText(line);
    }
}
