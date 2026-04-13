using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using IPSetupTool.Models;

namespace IPSetupTool.Services;

public class UDPDiscoveryService
{
    private const int DiscoveryPort = 5051;
    private const int TimeoutMs = 5000;

    private static readonly string[] DefaultKnownIPs = { "192.168.1.200" };

    // Extra IPs to try (set by SaveAndReboot with the new IP)
    private readonly List<string> _extraUnicastIPs = new();

    private static readonly string LogPath = Path.Combine(
        AppDomain.CurrentDomain.BaseDirectory, "discovery_log.txt");

    private static void Log(string msg)
    {
        var line = $"[{DateTime.Now:HH:mm:ss.fff}] {msg}";
        try { File.AppendAllText(LogPath, line + Environment.NewLine); } catch { }
    }

    public void AddUnicastTarget(string ip)
    {
        if (!_extraUnicastIPs.Contains(ip))
            _extraUnicastIPs.Add(ip);
    }

    public void ClearExtraTargets() => _extraUnicastIPs.Clear();

    public async Task<List<DeviceInfo>> DiscoverDevicesAsync()
    {
        var devices = new List<DeviceInfo>();
        var request = JsonSerializer.Serialize(new { cmd = "DISCOVER", product = "RemoteDeck" });
        var requestBytes = Encoding.UTF8.GetBytes(request);

        Log("=== Discovery Start ===");

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;
        udp.Client.ReceiveTimeout = TimeoutMs;

        // Broadcast
        await udp.SendAsync(requestBytes, requestBytes.Length,
            new IPEndPoint(IPAddress.Broadcast, DiscoveryPort));
        Log("  Sent broadcast");

        // Unicast to all known IPs
        var allTargets = new HashSet<string>(DefaultKnownIPs);
        foreach (var ip in _extraUnicastIPs) allTargets.Add(ip);

        foreach (var ip in allTargets)
        {
            try
            {
                await udp.SendAsync(requestBytes, requestBytes.Length,
                    new IPEndPoint(IPAddress.Parse(ip), DiscoveryPort));
                Log($"  Sent unicast to {ip}");
            }
            catch { }
        }

        // Receive
        var deadline = DateTime.UtcNow.AddMilliseconds(TimeoutMs);
        while (DateTime.UtcNow < deadline)
        {
            try
            {
                var remaining = deadline - DateTime.UtcNow;
                if (remaining <= TimeSpan.Zero) break;

                using var cts = new CancellationTokenSource(remaining);
                var result = await udp.ReceiveAsync(cts.Token);
                var json = Encoding.UTF8.GetString(result.Buffer);

                Log($"  Received {result.Buffer.Length}b from {result.RemoteEndPoint}");

                using var doc = JsonDocument.Parse(json);
                if (!doc.RootElement.TryGetProperty("cmd", out var cmdProp)) continue;
                if (cmdProp.GetString() != "DISCOVER_ACK") continue;

                var device = JsonSerializer.Deserialize<DeviceInfo>(json);
                if (device != null && !devices.Any(d => d.DeviceId == device.DeviceId))
                {
                    devices.Add(device);
                    Log($"  FOUND: {device.DeviceId} at {device.IP}");
                    deadline = DateTime.UtcNow.AddMilliseconds(1000);
                }
            }
            catch (OperationCanceledException) { break; }
            catch (SocketException) { break; }
            catch { }
        }

        Log($"=== Discovery End === Total: {devices.Count}");
        return devices;
    }

    // Auth check result from last GetConfig call (same socket)
    public bool LastAuthCheckResult { get; private set; } = false;

    public async Task<DeviceConfigRoot?> GetConfigViaUDPAsync(DeviceInfo device, string? authUser = null, string? authPass = null)
    {
        var payload = Encoding.UTF8.GetBytes(
            JsonSerializer.Serialize(new { cmd = "GET_CONFIG", device_id = device.DeviceId }));
        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        LastAuthCheckResult = false;

        // ARP warmup before first attempt
        Log($"GetConfig: ARP warmup ping to {device.IP}");
        using (var ping = new Ping())
        {
            for (int i = 0; i < 3; i++)
            {
                try
                {
                    var reply = await ping.SendPingAsync(device.IP, 1000);
                    if (reply.Status == IPStatus.Success)
                    {
                        Log($"  Ping OK ({reply.RoundtripTime}ms)");
                        break;
                    }
                }
                catch { }
            }
        }
        await Task.Delay(200);

        for (int attempt = 1; attempt <= 3; attempt++)
        {
            Log($"GetConfig attempt {attempt}/3 to {device.IP}");

            try
            {
                using var udp = new UdpClient(0);
                udp.EnableBroadcast = true;
                udp.Client.ReceiveTimeout = TimeoutMs;

                await udp.SendAsync(payload, payload.Length, ep);

                var deadline = DateTime.UtcNow.AddMilliseconds(TimeoutMs);
                while (DateTime.UtcNow < deadline)
                {
                    var remaining = deadline - DateTime.UtcNow;
                    if (remaining <= TimeSpan.Zero) break;

                    using var cts = new CancellationTokenSource(remaining);
                    var result = await udp.ReceiveAsync(cts.Token);
                    var json = Encoding.UTF8.GetString(result.Buffer);

                    Log($"  Received {result.Buffer.Length}b");

                    using var doc = JsonDocument.Parse(json);
                    if (doc.RootElement.TryGetProperty("config", out var configEl))
                    {
                        Log("  GET_CONFIG OK");
                        var configResult = JsonSerializer.Deserialize<DeviceConfigRoot>(configEl.GetRawText());

                        // Auth check on same socket (ARP already resolved)
                        if (authUser != null && authPass != null)
                        {
                            var authPayload = Encoding.UTF8.GetBytes(JsonSerializer.Serialize(
                                new { cmd = "AUTH_CHECK", device_id = device.DeviceId,
                                      auth = new { user = authUser, pass = authPass } }));
                            await udp.SendAsync(authPayload, authPayload.Length, ep);
                            try
                            {
                                var authResult = await udp.ReceiveAsync(new CancellationTokenSource(3000).Token);
                                var authJson = Encoding.UTF8.GetString(authResult.Buffer);
                                Log($"  AUTH_CHECK on same socket: {authJson}");
                                LastAuthCheckResult = authJson.Contains("\"ok\"");
                            }
                            catch { Log("  AUTH_CHECK timeout on same socket"); }
                        }

                        return configResult;
                    }
                }
            }
            catch (Exception ex)
            {
                Log($"  Attempt {attempt} failed: {ex.GetType().Name}");
            }

            if (attempt < 3) await Task.Delay(1000);
        }

        Log("GetConfig: All attempts failed");
        return null;
    }

    public async Task<bool> SendConfigAsync(DeviceInfo device, DeviceConfigRoot config)
    {
        var payload = new { cmd = "SET_CONFIG", device_id = device.DeviceId, config };
        var json = JsonSerializer.Serialize(payload);
        var bytes = Encoding.UTF8.GetBytes(json);

        Log($"SendConfig to {device.IP} ({bytes.Length} bytes)");

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;
        udp.Client.ReceiveTimeout = 10000;

        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        await udp.SendAsync(bytes, bytes.Length, ep);

        try
        {
            var result = await udp.ReceiveAsync(new CancellationTokenSource(10000).Token);
            var response = Encoding.UTF8.GetString(result.Buffer);
            Log($"SendConfig ACK: {response}");
            using var doc = JsonDocument.Parse(response);
            if (doc.RootElement.TryGetProperty("result", out var resultProp))
                return resultProp.GetString() == "ok";
            return true;
        }
        catch (Exception ex)
        {
            Log($"SendConfig failed: {ex.GetType().Name} - {ex.Message}");
            return false;
        }
    }

    public async Task<bool> SendRebootAsync(DeviceInfo device)
    {
        var payload = JsonSerializer.Serialize(new { cmd = "REBOOT", device_id = device.DeviceId });
        var bytes = Encoding.UTF8.GetBytes(payload);

        Log($"SendReboot to {device.IP}");

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;

        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        await udp.SendAsync(bytes, bytes.Length, ep);

        try
        {
            var result = await udp.ReceiveAsync(new CancellationTokenSource(2000).Token);
            Log("Reboot ACK received");
            return true;
        }
        catch
        {
            Log("Reboot ACK timeout (expected)");
            return true;
        }
    }

    // ─── Auth versions (v2.1) ──────────────────────────────

    public async Task<bool> CheckAuthAsync(DeviceInfo device, string authUser, string authPass)
    {
        var payload = Encoding.UTF8.GetBytes(
            JsonSerializer.Serialize(new { cmd = "AUTH_CHECK", device_id = device.DeviceId,
                auth = new { user = authUser, pass = authPass } }));
        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);

        Log($"AuthCheck to {device.IP} (user={authUser})");

        // ARP warmup
        using (var ping = new Ping())
        { for (int i = 0; i < 3; i++) { try { var r = await ping.SendPingAsync(device.IP, 1000); if (r.Status == IPStatus.Success) break; } catch { } } }
        await Task.Delay(200);

        for (int attempt = 1; attempt <= 3; attempt++)
        {
            try
            {
                using var udp = new UdpClient(0);
                udp.EnableBroadcast = true;
                udp.Client.ReceiveTimeout = 3000;
                await udp.SendAsync(payload, payload.Length, ep);

                var result = await udp.ReceiveAsync(new CancellationTokenSource(3000).Token);
                var json = Encoding.UTF8.GetString(result.Buffer);
                Log($"AuthCheck response: {json}");
                return json.Contains("\"ok\"");
            }
            catch (Exception ex)
            {
                Log($"AuthCheck attempt {attempt} failed: {ex.GetType().Name}");
                if (attempt < 3) await Task.Delay(500);
            }
        }
        Log("AuthCheck: All attempts failed");
        return false;
    }

    public async Task<bool> SendConfigWithAuthAsync(DeviceInfo device, DeviceConfigRoot config, string authUser, string authPass)
    {
        var payload = new
        {
            cmd = "SET_CONFIG",
            device_id = device.DeviceId,
            auth = new { user = authUser, pass = authPass },
            config
        };

        var json = JsonSerializer.Serialize(payload);
        var bytes = Encoding.UTF8.GetBytes(json);

        Log($"SendConfigWithAuth to {device.IP} ({bytes.Length} bytes)");

        // ARP warmup
        using (var ping = new Ping())
        { for (int i = 0; i < 3; i++) { try { var r = await ping.SendPingAsync(device.IP, 1000); if (r.Status == IPStatus.Success) break; } catch { } } }
        await Task.Delay(200);

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;
        udp.Client.ReceiveTimeout = 5000;

        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        await udp.SendAsync(bytes, bytes.Length, ep);

        try
        {
            var result = await udp.ReceiveAsync(new CancellationTokenSource(5000).Token);
            var response = Encoding.UTF8.GetString(result.Buffer);
            Log($"SendConfig ACK: {response}");
            using var doc = JsonDocument.Parse(response);
            if (doc.RootElement.TryGetProperty("result", out var resultProp))
            {
                var r = resultProp.GetString();
                if (r == "auth_failed") { Log("SendConfig: AUTH FAILED"); return false; }
                return r == "ok";
            }
            return true;
        }
        catch (Exception ex)
        {
            Log($"SendConfig failed: {ex.GetType().Name}");
            return false;
        }
    }

    public async Task<bool> SendRebootWithAuthAsync(DeviceInfo device, string authUser, string authPass)
    {
        var payload = JsonSerializer.Serialize(new
        {
            cmd = "REBOOT",
            device_id = device.DeviceId,
            auth = new { user = authUser, pass = authPass }
        });
        var bytes = Encoding.UTF8.GetBytes(payload);

        Log($"SendRebootWithAuth to {device.IP}");

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;

        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        await udp.SendAsync(bytes, bytes.Length, ep);

        try
        {
            var result = await udp.ReceiveAsync(new CancellationTokenSource(2000).Token);
            var response = Encoding.UTF8.GetString(result.Buffer);
            if (response.Contains("auth_failed")) return false;
            return true;
        }
        catch { return true; }
    }

    public async Task<bool> ChangeAuthAsync(DeviceInfo device, string curUser, string curPass, string newUser, string newPass)
    {
        var payload = JsonSerializer.Serialize(new
        {
            cmd = "CHANGE_AUTH",
            device_id = device.DeviceId,
            auth = new { user = curUser, pass = curPass },
            new_auth = new { user = newUser, pass = newPass }
        });
        var bytes = Encoding.UTF8.GetBytes(payload);

        Log($"ChangeAuth to {device.IP}");

        using var udp = new UdpClient(0);
        udp.EnableBroadcast = true;
        udp.Client.ReceiveTimeout = 5000;

        var ep = new IPEndPoint(IPAddress.Parse(device.IP), DiscoveryPort);
        await udp.SendAsync(bytes, bytes.Length, ep);

        try
        {
            var result = await udp.ReceiveAsync(new CancellationTokenSource(5000).Token);
            var response = Encoding.UTF8.GetString(result.Buffer);
            Log($"ChangeAuth response: {response}");
            return response.Contains("\"ok\"");
        }
        catch (Exception ex)
        {
            Log($"ChangeAuth failed: {ex.GetType().Name}");
            return false;
        }
    }
}
