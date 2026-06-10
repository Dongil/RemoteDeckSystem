using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using IPSetupTool.Models;

namespace IPSetupTool.Services;

public class DeviceConfigService
{
    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(5) };
    private static readonly HttpClient _httpLong = new() { Timeout = TimeSpan.FromSeconds(15) };

    public const string DefaultAuthUser = "admin";
    public const string DefaultAuthPass = "12345";

    private static readonly string LogPath = Path.Combine(
        AppDomain.CurrentDomain.BaseDirectory, "http_config_log.txt");

    private static void Log(string msg)
    {
        var line = $"[{DateTime.Now:HH:mm:ss.fff}] {msg}";
        try { File.AppendAllText(LogPath, line + Environment.NewLine); } catch { }
    }

    private static AuthenticationHeaderValue BasicAuth(string user, string pass)
    {
        var bytes = Encoding.UTF8.GetBytes($"{user}:{pass}");
        return new AuthenticationHeaderValue("Basic", Convert.ToBase64String(bytes));
    }

    public async Task<DeviceConfigRoot?> GetConfigAsync(string ip, int port = 5050, string user = DefaultAuthUser, string pass = DefaultAuthPass)
    {
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, $"http://{ip}:{port}/api/config");
            req.Headers.Authorization = BasicAuth(user, pass);
            var resp = await _http.SendAsync(req);
            if (!resp.IsSuccessStatusCode) { Log($"GetConfig {ip}:{port} -> {(int)resp.StatusCode}"); return null; }
            var body = await resp.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<DeviceConfigRoot>(body);
        }
        catch (Exception ex)
        {
            Log($"GetConfig {ip}:{port} failed: {ex.GetType().Name}");
            return null;
        }
    }

    public async Task<bool> TestConnectionAsync(string ip, int port = 5050, string user = DefaultAuthUser, string pass = DefaultAuthPass)
    {
        try
        {
            using var req = new HttpRequestMessage(HttpMethod.Get, $"http://{ip}:{port}/api/status");
            req.Headers.Authorization = BasicAuth(user, pass);
            var resp = await _http.SendAsync(req);
            return resp.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public async Task<bool> SendConfigViaHttpAsync(string ip, int port, DeviceConfigRoot config, string user = DefaultAuthUser, string pass = DefaultAuthPass)
    {
        try
        {
            var json = JsonSerializer.Serialize(config);
            Log($"SendConfig HTTP -> {ip}:{port}/api/config ({json.Length} bytes)");
            using var req = new HttpRequestMessage(HttpMethod.Post, $"http://{ip}:{port}/api/config")
            {
                Content = new StringContent(json, Encoding.UTF8, "application/json")
            };
            req.Headers.Authorization = BasicAuth(user, pass);
            var resp = await _httpLong.SendAsync(req);
            var body = await resp.Content.ReadAsStringAsync();
            Log($"SendConfig HTTP <- {(int)resp.StatusCode} {body}");
            if (!resp.IsSuccessStatusCode) return false;
            return body.Contains("\"ok\":true");
        }
        catch (Exception ex)
        {
            Log($"SendConfig HTTP {ip}:{port} failed: {ex.GetType().Name} - {ex.Message}");
            return false;
        }
    }

    public async Task<bool> SendRebootViaHttpAsync(string ip, int port, string user = DefaultAuthUser, string pass = DefaultAuthPass)
    {
        // Reboot 요청은 펌웨어가 응답 후 즉시 restart하므로 응답 도달이 안 될 수 있음.
        // 짧은 timeout + TaskCanceledException을 성공으로 처리.
        try
        {
            Log($"SendReboot HTTP -> {ip}:{port}/api/reboot");
            using var req = new HttpRequestMessage(HttpMethod.Post, $"http://{ip}:{port}/api/reboot");
            req.Headers.Authorization = BasicAuth(user, pass);
            using var cts = new CancellationTokenSource(TimeSpan.FromSeconds(3));
            try
            {
                var resp = await _http.SendAsync(req, cts.Token);
                Log($"SendReboot HTTP <- {(int)resp.StatusCode}");
                return resp.IsSuccessStatusCode;
            }
            catch (TaskCanceledException)
            {
                Log($"SendReboot HTTP timeout (device likely rebooting - treating as success)");
                return true;
            }
        }
        catch (Exception ex)
        {
            Log($"SendReboot HTTP {ip}:{port} failed: {ex.GetType().Name} - {ex.Message}");
            return false;
        }
    }

    public DeviceConfigRoot BuildConfigForIPSetup(
        string deviceId, string deviceName, string mode,
        bool ethDhcp, string ethIp, string ethGateway, string ethSubnet, string ethDns1,
        string wifiSsid, string wifiPassword, bool wifiDhcp,
        string wifiIp, string wifiGateway, string wifiSubnet, string wifiDns)
    {
        return new DeviceConfigRoot
        {
            DeviceId = deviceId,
            DeviceName = deviceName,
            Network = new NetworkSection
            {
                Mode = mode,
                Ethernet = new EthernetConfig { Dhcp = ethDhcp, IP = ethIp, Gateway = ethGateway, Subnet = ethSubnet, Dns1 = ethDns1 },
                Wifi = new WifiConfig { Ssid = wifiSsid, Password = wifiPassword, Dhcp = wifiDhcp, IP = wifiIp, Gateway = wifiGateway, Subnet = wifiSubnet, Dns1 = wifiDns }
            }
        };
    }

    public DeviceConfigRoot BuildConfigFromFormV21(
        string deviceId, string mode,
        bool ethDhcp, string ethIp, string ethGateway, string ethSubnet, string ethDns1,
        string wifiSsid, string wifiPassword, bool wifiDhcp,
        string wifiIp, string wifiGateway, string wifiSubnet, string wifiDns,
        string mqttBroker, int mqttPort, string mqttUser, string mqttPassword)
    {
        return new DeviceConfigRoot
        {
            DeviceId = deviceId,
            Network = new NetworkSection
            {
                Mode = mode,
                Ethernet = new EthernetConfig
                {
                    Dhcp = ethDhcp,
                    IP = ethIp,
                    Gateway = ethGateway,
                    Subnet = ethSubnet,
                    Dns1 = ethDns1
                },
                Wifi = new WifiConfig
                {
                    Ssid = wifiSsid,
                    Password = wifiPassword,
                    Dhcp = wifiDhcp,
                    IP = wifiIp,
                    Gateway = wifiGateway,
                    Subnet = wifiSubnet,
                    Dns1 = wifiDns
                }
            },
            Mqtt = new MqttSection
            {
                Broker = mqttBroker,
                Port = mqttPort,
                User = mqttUser,
                Password = mqttPassword
            }
        };
    }
}
