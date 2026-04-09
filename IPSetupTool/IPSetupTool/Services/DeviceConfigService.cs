using System.Net.Http;
using System.Text.Json;
using IPSetupTool.Models;

namespace IPSetupTool.Services;

public class DeviceConfigService
{
    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(5) };

    public async Task<DeviceConfigRoot?> GetConfigAsync(string ip, int port = 5050)
    {
        try
        {
            var response = await _http.GetStringAsync($"http://{ip}:{port}/api/config");
            return JsonSerializer.Deserialize<DeviceConfigRoot>(response);
        }
        catch
        {
            return null;
        }
    }

    public async Task<bool> TestConnectionAsync(string ip, int port = 5050)
    {
        try
        {
            var response = await _http.GetAsync($"http://{ip}:{port}/api/status");
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    public DeviceConfigRoot BuildConfigForIPSetup(
        string deviceId, string deviceName, string mode,
        bool ethDhcp, string ethIp, string ethGateway, string ethSubnet, string ethDns1,
        string wifiSsid, string wifiPassword, bool wifiDhcp,
        string wifiIp, string wifiGateway, string wifiSubnet, string wifiDns)
    {
        return new DeviceConfigRoot
        {
            DeviceId = deviceId,
            DeviceName = deviceName,
            Network = new NetworkSection
            {
                Mode = mode,
                Ethernet = new EthernetConfig { Dhcp = ethDhcp, IP = ethIp, Gateway = ethGateway, Subnet = ethSubnet, Dns1 = ethDns1 },
                Wifi = new WifiConfig { Ssid = wifiSsid, Password = wifiPassword, Dhcp = wifiDhcp, IP = wifiIp, Gateway = wifiGateway, Subnet = wifiSubnet, Dns1 = wifiDns }
            }
        };
    }

    public DeviceConfigRoot BuildConfigFromFormV21(
        string deviceId, string mode,
        bool ethDhcp, string ethIp, string ethGateway, string ethSubnet, string ethDns1,
        string wifiSsid, string wifiPassword, bool wifiDhcp,
        string wifiIp, string wifiGateway, string wifiSubnet, string wifiDns,
        string mqttBroker, int mqttPort, string mqttUser, string mqttPassword)
    {
        return new DeviceConfigRoot
        {
            DeviceId = deviceId,
            Network = new NetworkSection
            {
                Mode = mode,
                Ethernet = new EthernetConfig
                {
                    Dhcp = ethDhcp,
                    IP = ethIp,
                    Gateway = ethGateway,
                    Subnet = ethSubnet,
                    Dns1 = ethDns1
                },
                Wifi = new WifiConfig
                {
                    Ssid = wifiSsid,
                    Password = wifiPassword,
                    Dhcp = wifiDhcp,
                    IP = wifiIp,
                    Gateway = wifiGateway,
                    Subnet = wifiSubnet,
                    Dns1 = wifiDns
                }
            },
            Mqtt = new MqttSection
            {
                Broker = mqttBroker,
                Port = mqttPort,
                User = mqttUser,
                Password = mqttPassword
            }
        };
    }
}
