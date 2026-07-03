// Design Ref: RemoteDeck_PC_v2.5 §5.5 — Per-device log poller. Mirrors DevicePoller's
// pattern (per-device Task + PeriodicTimer + CancellationToken) so one offline device
// does not block others. Merges fresh entries into a per-device list keeping order
// (server-side reversal handled at UI). Dedup by (Timestamp,Event,Detail). Cap at 500.
using System.Collections.Concurrent;
using IntegrateController.Models;

namespace IntegrateController.Services;

public sealed class LogPoller : IDisposable
{
    private readonly RemoteDeckClient _client;
    private readonly Dictionary<string, PollEntry> _entries = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, DeviceLogState> _logs = new(StringComparer.Ordinal);
    private readonly object _lock = new();

    public int PollIntervalSec { get; set; } = 5;    // Plan NFR-4
    public int MaxEntriesPerDevice { get; set; } = 500;  // Design §12
    public int StaggerMs { get; set; } = 300;         // Design §5.5

    public event EventHandler<LogsUpdatedEventArgs>? LogsUpdated;

    public LogPoller(RemoteDeckClient client)
    {
        _client = client;
    }

    public void Start(DeviceEntry device, int staggerIndex)
    {
        lock (_lock)
        {
            if (_entries.ContainsKey(device.Id)) return;

            var cts = new CancellationTokenSource();
            var startDelayMs = staggerIndex * StaggerMs;
            var task = Task.Run(() => PollLoopAsync(device, startDelayMs, cts.Token));
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
        _logs.TryRemove(deviceId, out _);
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
        _logs.Clear();
    }

    // Returns latest N entries for the given device, newest-first.
    public IReadOnlyList<LogEntry> GetLogs(string deviceId, int max = 100)
    {
        if (!_logs.TryGetValue(deviceId, out var state)) return Array.Empty<LogEntry>();
        lock (state.Lock)
        {
            // state.Entries is stored oldest-first (same order as RD_PC). Reverse for display.
            var count = Math.Min(max, state.Entries.Count);
            var result = new LogEntry[count];
            for (int i = 0; i < count; i++)
                result[i] = state.Entries[state.Entries.Count - 1 - i];
            return result;
        }
    }

    private async Task PollLoopAsync(DeviceEntry device, int startDelayMs, CancellationToken ct)
    {
        if (startDelayMs > 0)
        {
            try { await Task.Delay(startDelayMs, ct).ConfigureAwait(false); }
            catch (OperationCanceledException) { return; }
        }

        var interval = TimeSpan.FromSeconds(Math.Clamp(PollIntervalSec, 1, 60));
        using var timer = new PeriodicTimer(interval);

        // Immediate first poll.
        await PollOnceAndMergeAsync(device, ct).ConfigureAwait(false);

        while (!ct.IsCancellationRequested)
        {
            try
            {
                if (!await timer.WaitForNextTickAsync(ct).ConfigureAwait(false)) return;
                await PollOnceAndMergeAsync(device, ct).ConfigureAwait(false);
            }
            catch (OperationCanceledException) { return; }
            catch (Exception ex)
            {
                System.Diagnostics.Trace.WriteLine($"[LogPoller {device.Ip}] {ex.GetType().Name}: {ex.Message}");
            }
        }
    }

    private async Task PollOnceAndMergeAsync(DeviceEntry device, CancellationToken ct)
    {
        var result = await _client.GetLogsAsync(device, ct).ConfigureAwait(false);
        if (!result.Ok || result.Value == null) return;

        var state = _logs.GetOrAdd(device.Id, _ => new DeviceLogState());
        bool changed = false;
        lock (state.Lock)
        {
            foreach (var e in result.Value)
            {
                if (state.Seen.Add(e.DedupKey))
                {
                    state.Entries.Add(e);
                    changed = true;
                }
            }
            if (state.Entries.Count > MaxEntriesPerDevice)
            {
                int overflow = state.Entries.Count - MaxEntriesPerDevice;
                // Drop oldest but keep Seen — allows dedup vs long-running device with wrap-around
                // (millis wraps ~49d — for a 500-entry ring at typical rate this is fine).
                state.Entries.RemoveRange(0, overflow);
            }
        }
        if (changed) LogsUpdated?.Invoke(this, new LogsUpdatedEventArgs(device.Id));
    }

    public void Dispose()
    {
        StopAll();
    }

    private sealed record PollEntry(DeviceEntry Device, CancellationTokenSource Cts, Task Task);

    private sealed class DeviceLogState
    {
        public readonly object Lock = new();
        public readonly List<LogEntry> Entries = new();
        public readonly HashSet<string> Seen = new(StringComparer.Ordinal);
    }
}

public sealed class LogsUpdatedEventArgs : EventArgs
{
    public string DeviceId { get; }
    public LogsUpdatedEventArgs(string deviceId) { DeviceId = deviceId; }
}
