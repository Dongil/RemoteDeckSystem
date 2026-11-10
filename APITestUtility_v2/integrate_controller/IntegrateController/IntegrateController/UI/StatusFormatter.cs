// Design Ref: §5 — DataGridView cell rendering helpers (status emoji, colors, uptime).
using System.Drawing;
using IntegrateController.Models;

namespace IntegrateController.UI;

internal static class StatusFormatter
{
    public static readonly Color OnlineBg     = Color.FromArgb(220, 252, 220);
    public static readonly Color OfflineBg    = Color.FromArgb(252, 220, 220);
    public static readonly Color WarningBg    = Color.FromArgb(252, 248, 200);
    public static readonly Color DefaultBg    = Color.White;

    // Selection variants — preserve the online/offline tint when a row is selected.
    public static readonly Color OnlineSelBg  = Color.FromArgb(150, 215, 150);
    public static readonly Color OfflineSelBg = Color.FromArgb(220, 150, 150);
    public static readonly Color WarningSelBg = Color.FromArgb(225, 205, 100);
    public static readonly Color DefaultSelBg = SystemColors.Highlight;

    public static string StatusGlyph(DeviceStatus? s)
    {
        if (s == null) return "○";
        if (!s.Online) return "●";
        return "●";
    }

    public static Color StatusColor(DeviceStatus? s)
    {
        if (s == null) return Color.Gray;
        if (!s.Online) return Color.Firebrick;
        return Color.ForestGreen;
    }

    public static Color RowBackColor(DeviceStatus? s, bool authIssue)
    {
        if (authIssue) return WarningBg;
        if (s == null) return DefaultBg;
        if (!s.Online) return OfflineBg;
        return OnlineBg;
    }

    public static Color RowSelectionBackColor(DeviceStatus? s, bool authIssue)
    {
        if (authIssue) return WarningSelBg;
        if (s == null) return DefaultSelBg;
        if (!s.Online) return OfflineSelBg;
        return OnlineSelBg;
    }

    public static string FormatBool(bool b) => b ? "ON" : "OFF";
    public static string FormatPc(DeviceStatus? s) => s == null ? "?" : (s.PcOn ? "ON" : "OFF");

    // v2.6.2: RemoteDeck_PC v2.6.2 /api/status.attendance 렌더링
    //   enabled=false → 미설정
    //   current=present → 재실 / absent → 부재 / unknown → 미설정
    public static string FormatAttendance(DeviceStatus? s)
    {
        if (s == null) return "-";
        if (!s.AttendanceEnabled) return "미설정";
        return s.AttendanceCurrent switch
        {
            "present" => "재실",
            "absent"  => "부재",
            _          => "미설정",
        };
    }
    // v3.0.0: RemoteDeck_PC v3.0.0 /api/status hub_* 렌더링 (ic-hub-v3.0 Design §4.2) — 위에서부터 먼저 맞는 규칙
    public static string FormatHub(DeviceStatus? s)
    {
        if (s == null || !s.Online) return "-";
        if (!s.HubSupported) return "미지원";
        if (!s.HubEnabled) return "미사용";
        if (s.HubConnected) return "● 연결됨";
        if (s.HubStopped) return "● 정지";
        if (s.HubStopReason == "config incomplete") return "● 설정 불완전";
        return "● 연결 중";
    }

    public static Color HubColor(DeviceStatus? s)
    {
        if (s == null || !s.Online) return Color.Empty;   // Empty = 행 기본 글자색 상속
        if (!s.HubSupported || !s.HubEnabled) return Color.Gray;
        if (s.HubConnected) return Color.ForestGreen;
        if (s.HubStopped || s.HubStopReason == "config incomplete") return Color.Firebrick;
        return Color.DarkOrange;
    }

    // v3.0.0: 정지 사유 한글 (장비 웹 UI app.js HUB_STOP_TEXT와 같은 문구, Design §4.3)
    public static string HubStopText(string reason)
    {
        if (string.IsNullOrEmpty(reason)) return "";
        if (reason == "config incomplete") return "허브 주소·제품번호·장비 키 입력 필요";
        var code = reason.Split(' ')[0];
        return code switch
        {
            "4001" => "장비 키 또는 제품번호 오류 (4001)",
            "4009" => "같은 제품번호 장비가 이미 접속 중 (4009)",
            "4010" => "버전 불일치 (4010)",
            _      => reason,
        };
    }

    // v3.0.0: 상세 패널 요약 1줄 (Design §5.4)
    public static string FormatHubDetail(DeviceStatus s)
    {
        if (!s.HubSupported) return "RCM 허브: 미지원 (펌웨어 v3.0 미만)";
        if (!s.HubEnabled) return "RCM 허브: 미사용";
        var line = "RCM 허브: " + FormatHub(s);
        var why = HubStopText(s.HubStopReason);
        if (why.Length > 0) line += " — " + why;
        // 펌웨어는 host가 비어도 ":port"를 보내므로 host 부분이 없으면 "-"
        var host = s.HubHost.Length == 0 || s.HubHost.StartsWith(':') ? "-" : s.HubHost;
        return line + $"  주소 {host}  제품번호 {(s.HubProductId.Length > 0 ? s.HubProductId : "-")}";
    }

    public static string FormatGpio(DeviceStatus? s) => s == null ? "---" : s.GpioString;
    public static string FormatUptime(DeviceStatus? s) => s == null ? "-" : s.UptimeFormatted;
    public static string FormatFw(DeviceStatus? s) => s == null || string.IsNullOrEmpty(s.FwVer) ? "-" : s.FwVer;
    public static string FormatLastSeen(DeviceStatus? s) =>
        s?.LastSeen?.ToString("HH:mm:ss") ?? "-";
}
