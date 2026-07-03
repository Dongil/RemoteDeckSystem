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
    public static string FormatGpio(DeviceStatus? s) => s == null ? "---" : s.GpioString;
    public static string FormatUptime(DeviceStatus? s) => s == null ? "-" : s.UptimeFormatted;
    public static string FormatFw(DeviceStatus? s) => s == null || string.IsNullOrEmpty(s.FwVer) ? "-" : s.FwVer;
    public static string FormatLastSeen(DeviceStatus? s) =>
        s?.LastSeen?.ToString("HH:mm:ss") ?? "-";
}
