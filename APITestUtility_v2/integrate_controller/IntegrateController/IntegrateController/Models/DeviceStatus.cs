// Design Ref: §3.1, §4.2 — In-memory parsed /api/status payload. Not persisted.
namespace IntegrateController.Models;

public sealed class DeviceStatus
{
    public bool Online { get; set; }
    public DateTime? LastSeen { get; set; }

    public bool PcOn { get; set; }
    public bool Relay1 { get; set; }
    public bool Relay2 { get; set; }
    public int[] Gpio { get; set; } = Array.Empty<int>();
    public long UptimeSec { get; set; }
    public string Ip { get; set; } = "";
    public string Mac { get; set; } = "";
    public string NetMode { get; set; } = "";
    public string FwVer { get; set; } = "";
    public string DeviceName { get; set; } = "";
    public string DeviceId { get; set; } = "";
    public bool NtpSynced { get; set; }
    public string Time { get; set; } = "";
    public bool MqttConnected { get; set; }
    public long HeapFree { get; set; }
    public long HeapMin { get; set; }

    public string? LastError { get; set; }
    public int ConsecutiveFailures { get; set; }
    public string? RawJson { get; set; }

    public string GpioString => Gpio.Length == 0 ? "---" : string.Concat(Gpio.Select(v => v.ToString()));

    public string UptimeFormatted
    {
        get
        {
            if (UptimeSec <= 0) return "-";
            var t = TimeSpan.FromSeconds(UptimeSec);
            if (t.TotalDays >= 1) return $"{(int)t.TotalDays}d{t.Hours}h";
            if (t.TotalHours >= 1) return $"{(int)t.TotalHours}h{t.Minutes}m";
            return $"{t.Minutes}m{t.Seconds}s";
        }
    }
}
