// Design Ref: §4 — HTTP Basic Auth client against RemoteDeck_PC /api/status and /api/reboot.
// HttpClient is a singleton (one socket pool). Authorization header is set per request.
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using IntegrateController.Models;

namespace IntegrateController.Services;

public sealed record RestResult<T>(bool Ok, T? Value, int StatusCode, string? Error);

public sealed class DeviceConfigSummary
{
    public string DeviceId { get; set; } = "";
    public string DeviceName { get; set; } = "";
    public string Product { get; set; } = "";
}

public sealed class RemoteDeckClient
{
    // Singleton HttpClient. No per-device instance — avoids socket exhaustion.
    private static readonly HttpClient Http = new(new SocketsHttpHandler
    {
        PooledConnectionLifetime = TimeSpan.FromMinutes(2),
        AllowAutoRedirect = false
    });

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        PropertyNameCaseInsensitive = true
    };

    public async Task<RestResult<DeviceConfigSummary>> GetConfigAsync(DeviceEntry device, CancellationToken ct)
    {
        var url = $"{device.BaseUrl}/api/config";
        using var req = new HttpRequestMessage(HttpMethod.Get, url);
        AttachAuth(req, device);

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(device.TimeoutMs > 0 ? device.TimeoutMs : 2000);

        try
        {
            using var resp = await Http.SendAsync(req, HttpCompletionOption.ResponseHeadersRead, cts.Token)
                                       .ConfigureAwait(false);
            var body = await resp.Content.ReadAsStringAsync(cts.Token).ConfigureAwait(false);
            if (!resp.IsSuccessStatusCode)
            {
                return new RestResult<DeviceConfigSummary>(false, null, (int)resp.StatusCode,
                    resp.StatusCode == System.Net.HttpStatusCode.Unauthorized
                        ? "인증 실패 (401)"
                        : $"HTTP {(int)resp.StatusCode}");
            }

            var cfg = new DeviceConfigSummary();
            try
            {
                using var doc = JsonDocument.Parse(body);
                var root = doc.RootElement;
                if (root.TryGetProperty("device_id", out var v)) cfg.DeviceId = v.GetString() ?? "";
                if (root.TryGetProperty("device_name", out v)) cfg.DeviceName = v.GetString() ?? "";
                if (root.TryGetProperty("product", out v)) cfg.Product = v.GetString() ?? "";
            }
            catch (Exception ex)
            {
                return new RestResult<DeviceConfigSummary>(false, null, (int)resp.StatusCode, "parse: " + ex.Message);
            }
            return new RestResult<DeviceConfigSummary>(true, cfg, (int)resp.StatusCode, null);
        }
        catch (OperationCanceledException) when (cts.IsCancellationRequested && !ct.IsCancellationRequested)
        {
            return new RestResult<DeviceConfigSummary>(false, null, 0, "timeout");
        }
        catch (HttpRequestException ex)
        {
            return new RestResult<DeviceConfigSummary>(false, null, 0, ex.Message);
        }
        catch (Exception ex)
        {
            return new RestResult<DeviceConfigSummary>(false, null, 0, ex.GetType().Name + ": " + ex.Message);
        }
    }

    public async Task<RestResult<DeviceStatus>> GetStatusAsync(DeviceEntry device, CancellationToken ct)
    {
        var url = $"{device.BaseUrl}/api/status";
        using var req = new HttpRequestMessage(HttpMethod.Get, url);
        AttachAuth(req, device);

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(device.TimeoutMs > 0 ? device.TimeoutMs : 2000);

        try
        {
            using var resp = await Http.SendAsync(req, HttpCompletionOption.ResponseHeadersRead, cts.Token)
                                       .ConfigureAwait(false);
            var body = await resp.Content.ReadAsStringAsync(cts.Token).ConfigureAwait(false);
            if (!resp.IsSuccessStatusCode)
            {
                return new RestResult<DeviceStatus>(false, null, (int)resp.StatusCode,
                    resp.StatusCode == System.Net.HttpStatusCode.Unauthorized
                        ? "인증 실패 (401)"
                        : $"HTTP {(int)resp.StatusCode}");
            }

            var status = ParseStatus(body);
            return new RestResult<DeviceStatus>(true, status, (int)resp.StatusCode, null);
        }
        catch (OperationCanceledException) when (cts.IsCancellationRequested && !ct.IsCancellationRequested)
        {
            return new RestResult<DeviceStatus>(false, null, 0, "timeout");
        }
        catch (HttpRequestException ex)
        {
            return new RestResult<DeviceStatus>(false, null, 0, ex.Message);
        }
        catch (Exception ex)
        {
            return new RestResult<DeviceStatus>(false, null, 0, ex.GetType().Name + ": " + ex.Message);
        }
    }

    // RemoteDeck_PC_v2.5 §4.1 — GET /api/log. Response shape: {"logs":[{timestamp,time,event,detail}]}
    public async Task<RestResult<LogEntry[]>> GetLogsAsync(DeviceEntry device, CancellationToken ct)
    {
        var url = $"{device.BaseUrl}/api/log";
        using var req = new HttpRequestMessage(HttpMethod.Get, url);
        AttachAuth(req, device);

        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(device.TimeoutMs > 0 ? device.TimeoutMs : 2000);

        try
        {
            using var resp = await Http.SendAsync(req, HttpCompletionOption.ResponseHeadersRead, cts.Token)
                                       .ConfigureAwait(false);
            var body = await resp.Content.ReadAsStringAsync(cts.Token).ConfigureAwait(false);
            if (!resp.IsSuccessStatusCode)
            {
                return new RestResult<LogEntry[]>(false, null, (int)resp.StatusCode,
                    resp.StatusCode == System.Net.HttpStatusCode.Unauthorized
                        ? "인증 실패 (401)"
                        : $"HTTP {(int)resp.StatusCode}");
            }

            try
            {
                using var doc = JsonDocument.Parse(body);
                var root = doc.RootElement;
                if (!root.TryGetProperty("logs", out var arr) || arr.ValueKind != JsonValueKind.Array)
                {
                    return new RestResult<LogEntry[]>(true, Array.Empty<LogEntry>(), (int)resp.StatusCode, null);
                }
                var list = new List<LogEntry>();
                foreach (var e in arr.EnumerateArray())
                {
                    long ts = e.TryGetProperty("timestamp", out var t) && t.ValueKind == JsonValueKind.Number
                              ? t.GetInt64() : 0;
                    string time = e.TryGetProperty("time", out var tm) && tm.ValueKind == JsonValueKind.String
                                  ? tm.GetString() ?? "" : "";
                    string ev = e.TryGetProperty("event", out var ee) && ee.ValueKind == JsonValueKind.String
                                ? ee.GetString() ?? "" : "";
                    string det = e.TryGetProperty("detail", out var dd) && dd.ValueKind == JsonValueKind.String
                                 ? dd.GetString() ?? "" : "";
                    list.Add(new LogEntry(ts, time, ev, det));
                }
                return new RestResult<LogEntry[]>(true, list.ToArray(), (int)resp.StatusCode, null);
            }
            catch (Exception ex)
            {
                return new RestResult<LogEntry[]>(false, null, (int)resp.StatusCode, "parse: " + ex.Message);
            }
        }
        catch (OperationCanceledException) when (cts.IsCancellationRequested && !ct.IsCancellationRequested)
        {
            return new RestResult<LogEntry[]>(false, null, 0, "timeout");
        }
        catch (HttpRequestException ex)
        {
            return new RestResult<LogEntry[]>(false, null, 0, ex.Message);
        }
        catch (Exception ex)
        {
            return new RestResult<LogEntry[]>(false, null, 0, ex.GetType().Name + ": " + ex.Message);
        }
    }

    // Design Ref: §4.2 — RemoteDeck_PC firmware calls ESP.restart() immediately after
    // req->send(200,...) so the TCP response is usually NOT flushed before the device
    // reboots. We therefore treat timeout and connection-reset / aborted as SUCCESS
    // (firmware accepted the request and is rebooting). Only 401/4xx-non-200 are failures.
    public async Task<RestResult<bool>> RebootAsync(DeviceEntry device, CancellationToken ct)
    {
        var url = $"{device.BaseUrl}/api/reboot";
        using var req = new HttpRequestMessage(HttpMethod.Post, url)
        {
            Content = new StringContent("", Encoding.UTF8, "application/json")
        };
        AttachAuth(req, device);

        var timeoutMs = device.TimeoutMs > 0 ? device.TimeoutMs : 2000;
        using var cts = CancellationTokenSource.CreateLinkedTokenSource(ct);
        cts.CancelAfter(timeoutMs);

        try
        {
            using var resp = await Http.SendAsync(req, cts.Token).ConfigureAwait(false);
            if (resp.IsSuccessStatusCode)
            {
                return new RestResult<bool>(true, true, (int)resp.StatusCode, null);
            }
            if (resp.StatusCode == System.Net.HttpStatusCode.Unauthorized)
            {
                return new RestResult<bool>(false, false, 401, "인증 실패 (401)");
            }
            return new RestResult<bool>(false, false, (int)resp.StatusCode, $"HTTP {(int)resp.StatusCode}");
        }
        catch (OperationCanceledException) when (cts.IsCancellationRequested && !ct.IsCancellationRequested)
        {
            // Firmware rebooted before responding — treat as success.
            return new RestResult<bool>(true, true, 0, "응답 없이 reboot 추정 (펌웨어 flush 전 재시작)");
        }
        catch (HttpRequestException ex) when (IsRebootInProgress(ex))
        {
            // ECONNRESET / EPIPE / connection aborted while waiting for response.
            return new RestResult<bool>(true, true, 0, "응답 없이 reboot 추정 (연결 종료)");
        }
        catch (HttpRequestException ex)
        {
            return new RestResult<bool>(false, false, 0, ex.Message);
        }
        catch (Exception ex)
        {
            return new RestResult<bool>(false, false, 0, ex.GetType().Name + ": " + ex.Message);
        }
    }

    private static bool IsRebootInProgress(HttpRequestException ex)
    {
        // Distinguish "device rebooted mid-response" from "device unreachable".
        // Reachable + rebooting → typically SocketException ConnectionReset / ConnectionAborted.
        if (ex.InnerException is System.Net.Sockets.SocketException sx)
        {
            return sx.SocketErrorCode is System.Net.Sockets.SocketError.ConnectionReset
                                       or System.Net.Sockets.SocketError.ConnectionAborted
                                       or System.Net.Sockets.SocketError.NetworkReset
                                       or System.Net.Sockets.SocketError.Shutdown;
        }
        return false;
    }

    private static void AttachAuth(HttpRequestMessage req, DeviceEntry device)
    {
        var plain = DeviceStore.UnprotectPassword(device.AuthPasswordProtected) ?? "";
        var token = Convert.ToBase64String(Encoding.UTF8.GetBytes($"{device.AuthUser}:{plain}"));
        req.Headers.Authorization = new AuthenticationHeaderValue("Basic", token);
    }

    // Design Ref: §4.2 — Parse RemoteDeck_PC main.cpp buildStatusJson() shape.
    private static DeviceStatus ParseStatus(string json)
    {
        var status = new DeviceStatus
        {
            Online = true,
            LastSeen = DateTime.Now,
            RawJson = json
        };

        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            if (root.TryGetProperty("pc_on", out var v)) status.PcOn = v.GetBoolean();
            if (root.TryGetProperty("relay1", out v)) status.Relay1 = v.GetBoolean();
            if (root.TryGetProperty("relay2", out v)) status.Relay2 = v.GetBoolean();
            if (root.TryGetProperty("gpio", out v) && v.ValueKind == JsonValueKind.Array)
                status.Gpio = v.EnumerateArray().Select(e => e.GetInt32()).ToArray();
            if (root.TryGetProperty("uptime", out v)) status.UptimeSec = v.GetInt64();
            if (root.TryGetProperty("ip", out v)) status.Ip = v.GetString() ?? "";
            if (root.TryGetProperty("mac", out v)) status.Mac = v.GetString() ?? "";
            if (root.TryGetProperty("net_mode", out v)) status.NetMode = v.GetString() ?? "";
            if (root.TryGetProperty("fw_ver", out v)) status.FwVer = v.GetString() ?? "";
            if (root.TryGetProperty("device_name", out v)) status.DeviceName = v.GetString() ?? "";
            if (root.TryGetProperty("ntp_synced", out v)) status.NtpSynced = v.GetBoolean();
            if (root.TryGetProperty("time", out v)) status.Time = v.GetString() ?? "";
            if (root.TryGetProperty("mqtt_connected", out v)) status.MqttConnected = v.GetBoolean();
            if (root.TryGetProperty("heap_free", out v)) status.HeapFree = v.GetInt64();
            if (root.TryGetProperty("heap_min", out v)) status.HeapMin = v.GetInt64();

            // v2.6.3: RemoteDeck_PC v2.6.2 attendance mini block (하위호환 — 블록 없으면 unknown 기본)
            if (root.TryGetProperty("attendance", out var att) && att.ValueKind == JsonValueKind.Object)
            {
                if (att.TryGetProperty("enabled", out var e))  status.AttendanceEnabled = e.GetBoolean();
                if (att.TryGetProperty("source",  out var s2)) status.AttendanceSource  = s2.GetString() ?? "";
                if (att.TryGetProperty("current", out var c))  status.AttendanceCurrent = c.GetString() ?? "unknown";
            }
        }
        catch (Exception ex)
        {
            status.LastError = "parse: " + ex.Message;
        }

        return status;
    }
}
