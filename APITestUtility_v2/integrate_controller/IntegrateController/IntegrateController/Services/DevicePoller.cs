// Design Ref: §2.1, §6 — Per-device PeriodicTimer + CancellationToken. Independent task per device,
// so one offline device does not block others. Consecutive 3 failures = Offline.
using IntegrateController.Models;

namespace IntegrateController.Services;

public sealed class DevicePoller : IDisposable
{
    private readonly RemoteDeckClient _client;
    private readonly Dictionary<string, PollEntry> _entries = new(StringComparer.Ordinal);
    // device_id is fetched from /api/config once per device per session and cached.
    private readonly Dictionary<string, string> _deviceIdCache = new(StringComparer.Ordinal);
    private readonly object _lock = new();

    public event EventHandler<DeviceStatusUpdatedEventArgs>? StatusUpdated;

    public DevicePoller(RemoteDeckClient client)
    {
        _client = client;
    }

    public void Start(DeviceEntry device)
    {
        lock (_lock)
        {
            if (_entries.ContainsKey(device.Id)) return;

            var cts = new CancellationTokenSource();
            var task = Task.Run(() => PollLoopAsync(device, cts.Token));
            _entries[device.Id] = new PollEntry(device, cts, task);
        }
    }

    public void Stop(string deviceId)
    {
        PollEntry? entry;
        lock (_lock)
        {
            if (!_entries.TryGetValue(deviceId, out entry)) return;
            _entries.Remove(deviceId);
        }
        entry.Cts.Cancel();
    }

    public void Restart(DeviceEntry device)
    {
        Stop(device.Id);
        Start(device);
    }

    public void StopAll()
    {
        List<PollEntry> snapshot;
        lock (_lock)
        {
            snapshot = _entries.Values.ToList();
            _entries.Clear();
        }
        foreach (var e in snapshot) e.Cts.Cancel();
    }

    public bool IsRunning(string deviceId)
    {
        lock (_lock) return _entries.ContainsKey(deviceId);
    }

    public async Task<DeviceStatus> PollOnceAsync(DeviceEntry device, CancellationToken ct)
    {
        return await DoOnePollAsync(device, ct, currentFailures: 0).ConfigureAwait(false);
    }

    private async Task PollLoopAsync(DeviceEntry device, CancellationToken ct)
    {
        int failures = 0;
        var interval = TimeSpan.FromSeconds(Math.Clamp(device.PollIntervalSec, 1, 30));
        using var timer = new PeriodicTimer(interval);

        // Immediate first poll.
        try
        {
            var status = await DoOnePollAsync(device, ct, failures).ConfigureAwait(false);
            failures = status.Online ? 0 : failures + 1;
            status.ConsecutiveFailures = failures;
            status.Online = failures < 3;
            RaiseUpdated(device.Id, status);
        }
        catch (OperationCanceledException) { return; }

        while (!ct.IsCancellationRequested)
        {
            try
            {
                if (!await timer.WaitForNextTickAsync(ct).ConfigureAwait(false)) return;

                var status = await DoOnePollAsync(device, ct, failures).ConfigureAwait(false);
                failures = status.Online ? 0 : failures + 1;
                status.ConsecutiveFailures = failures;
                status.Online = failures < 3;
                RaiseUpdated(device.Id, status);
            }
            catch (OperationCanceledException) { return; }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine($"[DevicePoller {device.Ip}] {ex.GetType().Name}: {ex.Message}");
                failures++;
                var errStatus = new DeviceStatus
                {
                    Online = failures < 3,
                    LastSeen = DateTime.Now,
                    ConsecutiveFailures = failures,
                    LastError = ex.Message
                };
                RaiseUpdated(device.Id, errStatus);
            }
        }
    }

    private async Task<DeviceStatus> DoOnePollAsync(DeviceEntry device, CancellationToken ct, int currentFailures)
    {
        var result = await _client.GetStatusAsync(device, ct).ConfigureAwait(false);
        if (result.Ok && result.Value != null)
        {
            await EnrichWithDeviceIdAsync(device, result.Value, ct).ConfigureAwait(false);
            return result.Value;
        }
        return new DeviceStatus
        {
            Online = false,
            LastSeen = DateTime.Now,
            ConsecutiveFailures = currentFailures + 1,
            LastError = result.Error
        };
    }

    // First successful poll → fetch /api/config once to learn device_id. Cache for the
    // session. /api/status does not return device_id, so this is the only path.
    private async Task EnrichWithDeviceIdAsync(DeviceEntry device, DeviceStatus status, CancellationToken ct)
    {
        string? cached;
        lock (_lock)
        {
            _deviceIdCache.TryGetValue(device.Id, out cached);
        }

        if (!string.IsNullOrEmpty(cached))
        {
            status.DeviceId = cached;
            return;
        }

        try
        {
            var cfg = await _client.GetConfigAsync(device, ct).ConfigureAwait(false);
            if (cfg.Ok && cfg.Value != null && !string.IsNullOrEmpty(cfg.Value.DeviceId))
            {
                lock (_lock) _deviceIdCache[device.Id] = cfg.Value.DeviceId;
                status.DeviceId = cfg.Value.DeviceId;
            }
        }
        catch (OperationCanceledException) { }
        catch (Exception ex)
        {
            System.Diagnostics.Trace.WriteLine($"[DevicePoller] config fetch failed for {device.Ip}: {ex.Message}");
        }
    }

    public void InvalidateDeviceIdCache(string deviceId)
    {
        lock (_lock) _deviceIdCache.Remove(deviceId);
    }

    private void RaiseUpdated(string deviceId, DeviceStatus status)
    {
        StatusUpdated?.Invoke(this, new DeviceStatusUpdatedEventArgs(deviceId, status));
    }

    public void Dispose()
    {
        StopAll();
    }

    private sealed record PollEntry(DeviceEntry Device, CancellationTokenSource Cts, Task Task);
}

public sealed class DeviceStatusUpdatedEventArgs : EventArgs
{
    public string DeviceId { get; }
    public DeviceStatus Status { get; }
    public DeviceStatusUpdatedEventArgs(string deviceId, DeviceStatus status)
    {
        DeviceId = deviceId;
        Status = status;
    }
}
