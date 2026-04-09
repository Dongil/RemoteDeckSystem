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

    [JsonPropertyName("net_mode")]
    public string? NetMode { get; set; }

    [JsonPropertyName("eth_ip")]
    public string? EthIP { get; set; }

    [JsonIgnore]
    public IPAddress? DiscoveredVia { get; set; }

    /// <summary>
    /// IPSetupTool이 사용할 IP: WiFi 모드면 eth_ip(관리IP), 아니면 ip
    /// </summary>
    [JsonIgnore]
    public string ManagementIP => !string.IsNullOrEmpty(EthIP) ? EthIP : IP;
}
