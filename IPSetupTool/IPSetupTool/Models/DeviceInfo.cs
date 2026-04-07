using System.Net;
using System.Text.Json.Serialization;

namespace IPSetupTool.Models;

public class DeviceInfo
{
    [JsonPropertyName("device_id")]
    public string DeviceId { get; set; } = "";

    [JsonPropertyName("ip")]
    public string IP { get; set; } = "";

    [JsonPropertyName("mac")]
    public string MAC { get; set; } = "";

    [JsonPropertyName("fw_ver")]
    public string FirmwareVersion { get; set; } = "";

    [JsonPropertyName("product")]
    public string Product { get; set; } = "";

    [JsonPropertyName("web_port")]
    public int WebPort { get; set; } = 5050;

    // Not from JSON - set by discovery to remember which local interface found this device
    [JsonIgnore]
    public IPAddress? DiscoveredVia { get; set; }
}
