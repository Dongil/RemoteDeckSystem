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

    // v2.6.2: RemoteDeck_PC v2.6.2 /api/status.attendance mini block
    // enabled=false 또는 필드 부재 시 "unknown". current: "present" | "absent" | "unknown"
    public bool   AttendanceEnabled { get; set; }
    public string AttendanceSource  { get; set; } = "";
    public string AttendanceCurrent { get; set; } = "unknown";

    // v3.0.0: RemoteDeck_PC v3.0.0 /api/status hub_* (ic-hub-v3.0, Design §3.1)
    // HubSupported = hub_enabled 키 존재 (v3.0 이상). 없으면 v2.x → "미지원"
    public bool   HubSupported  { get; set; }
    public bool   HubEnabled    { get; set; }
    public bool   HubConnected  { get; set; }
    public bool   HubStopped    { get; set; }
    public string HubStopReason { get; set; } = "";   // "<code> <reason>" | "config incomplete" | ""
    public string HubHost       { get; set; } = "";   // "host:port"
    public string HubProductId  { get; set; } = "";

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

// v3.0.0: IC 그리드 기능 컬럼(재부재 자리) 표시 모드 — 재부재와 RCM 허브는 같은 현장에서 함께 쓰지 않음 (ic-hub-v3.0)
public enum FeatureColumnMode { Attendance, Hub }
