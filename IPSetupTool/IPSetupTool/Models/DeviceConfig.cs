using System.Text.Json.Serialization;

namespace IPSetupTool.Models;

public class DeviceConfigRoot
{
    [JsonPropertyName("device_id")]
    public string DeviceId { get; set; } = "node_1";

    [JsonPropertyName("device_name")]
    public string? DeviceName { get; set; } = "새기기";

    [JsonPropertyName("product")]
    public string Product { get; set; } = "RemoteDeck_PC";

    [JsonPropertyName("network")]
    public NetworkSection Network { get; set; } = new();

    [JsonPropertyName("mqtt")]
    public MqttSection Mqtt { get; set; } = new();

    [JsonPropertyName("relay")]
    public RelaySection Relay { get; set; } = new();

    [JsonPropertyName("monitor")]
    public MonitorSection Monitor { get; set; } = new();

    [JsonPropertyName("wol")]
    public WolSection Wol { get; set; } = new();

    [JsonPropertyName("ntp")]
    public NtpSection Ntp { get; set; } = new();

    [JsonPropertyName("firmware")]
    public FirmwareSection Firmware { get; set; } = new();
}

public class NetworkSection
{
    [JsonPropertyName("mode")]
    public string Mode { get; set; } = "ethernet";

    [JsonPropertyName("ethernet")]
    public EthernetConfig Ethernet { get; set; } = new();

    [JsonPropertyName("wifi")]
    public WifiConfig Wifi { get; set; } = new();
}

public class EthernetConfig
{
    [JsonPropertyName("dhcp")]
    public bool Dhcp { get; set; }

    [JsonPropertyName("ip")]
    public string IP { get; set; } = "192.168.1.200";

    [JsonPropertyName("gateway")]
    public string Gateway { get; set; } = "192.168.1.1";

    [JsonPropertyName("subnet")]
    public string Subnet { get; set; } = "255.255.255.0";

    [JsonPropertyName("dns1")]
    public string Dns1 { get; set; } = "8.8.8.8";

    [JsonPropertyName("dns2")]
    public string Dns2 { get; set; } = "8.8.4.4";

    [JsonPropertyName("mac")]
    public string MAC { get; set; } = "";
}

public class WifiConfig
{
    [JsonPropertyName("ssid")]
    public string Ssid { get; set; } = "";

    [JsonPropertyName("password")]
    public string Password { get; set; } = "";

    [JsonPropertyName("dhcp")]
    public bool Dhcp { get; set; } = true;

    [JsonPropertyName("ip")]
    public string IP { get; set; } = "";

    [JsonPropertyName("gateway")]
    public string Gateway { get; set; } = "";

    [JsonPropertyName("subnet")]
    public string Subnet { get; set; } = "255.255.255.0";

    [JsonPropertyName("dns1")]
    public string Dns1 { get; set; } = "8.8.8.8";

    [JsonPropertyName("mac")]
    public string MAC { get; set; } = "";
}

public class MqttSection
{
    [JsonPropertyName("broker")]
    public string Broker { get; set; } = "";

    [JsonPropertyName("port")]
    public int Port { get; set; } = 1883;

    [JsonPropertyName("user")]
    public string User { get; set; } = "";

    [JsonPropertyName("password")]
    public string Password { get; set; } = "";

    [JsonPropertyName("keepalive")]
    public int Keepalive { get; set; } = 120;

    [JsonPropertyName("topic_pub")]
    public string TopicPub { get; set; } = "RemoteDeck/server";

    [JsonPropertyName("topic_sub")]
    public string TopicSub { get; set; } = "RemoteDeck/[device_id]";
}

public class RelaySection
{
    [JsonPropertyName("pulse_short_ms")]
    public int PulseShortMs { get; set; } = 500;

    [JsonPropertyName("pulse_long_ms")]
    public int PulseLongMs { get; set; } = 5000;
}

public class MonitorSection
{
    [JsonPropertyName("pcled_poll_interval_ms")]
    public int PcledPollMs { get; set; } = 1000;

    [JsonPropertyName("auto_notify")]
    public bool AutoNotify { get; set; } = true;
}

public class WolSection
{
    [JsonPropertyName("target_mac")]
    public string TargetMac { get; set; } = "";
}

public class NtpSection
{
    [JsonPropertyName("server")]
    public string Server { get; set; } = "pool.ntp.org";

    [JsonPropertyName("timezone")]
    public string Timezone { get; set; } = "KST-9";
}

public class FirmwareSection
{
    [JsonPropertyName("version")]
    public string Version { get; set; } = "";

    [JsonPropertyName("date")]
    public string Date { get; set; } = "";
}
