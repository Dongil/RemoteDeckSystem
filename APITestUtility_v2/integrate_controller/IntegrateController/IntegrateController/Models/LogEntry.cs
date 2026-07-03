// Design Ref: RemoteDeck_PC_v2.5 §3.3 — Per-device log entry pulled from /api/log.
// Timestamp is RemoteDeck_PC millis() at emit time (uint32, ~49d wrap). Combined with
// EventStr + DetailStr it is used as the dedup key within a single device session.
namespace IntegrateController.Models;

public sealed record LogEntry(
    long   Timestamp,   // ms since RD_PC boot (from Logger.toJson field "timestamp")
    string TimeStr,     // NTP-formatted time string ("time" field, may be empty)
    string EventStr,    // "BOOT" / "RELAY" / "WEBREQ" / "MQTT" / ...
    string Detail
)
{
    public string DedupKey => $"{Timestamp}|{EventStr}|{Detail}";
}
