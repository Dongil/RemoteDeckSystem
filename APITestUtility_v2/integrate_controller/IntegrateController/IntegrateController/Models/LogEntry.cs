// Design Ref: RemoteDeck_PC_v2.5 §3.3 — Per-device log entry pulled from /api/log.
// Timestamp is RemoteDeck_PC millis() at emit time (uint32, ~49d wrap). Combined with
// EventStr + DetailStr it is used as the dedup key within a single device session.
namespace IntegrateController.Models;

public sealed record LogEntry(
    long   Timestamp,   // ms since RD_PC boot (from Logger.toJson field "timestamp")
    string TimeStr,     // NTP-formatted time string HH:MM:SS ("time" field, may be empty)
    string EventStr,    // "BOOT" / "RELAY" / "WEBREQ" / "MQTT" / ...
    string Detail
)
{
    // v2.6.3: device Logger는 HH:MM:SS만 저장 (no date). 클라이언트 수신 시각을 날짜 컬럼용으로 기록.
    public DateTime ReceivedAt { get; init; } = DateTime.Now;

    public string DedupKey => $"{Timestamp}|{EventStr}|{Detail}";
}
