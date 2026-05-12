using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Security.Principal;
using System.Text.Json;

namespace IPSetupTool.Services;

public class NicState
{
    public string NicName { get; set; } = "";
    public bool WasDhcp { get; set; }
    public bool WasDnsAuto { get; set; }
    public DateTime SavedAt { get; set; }
}

public static class NetworkHelper
{
    private const string TempIP = "192.168.1.100";
    private const string TempMask = "255.255.255.0";

    private static readonly string LogPath = Path.Combine(
        AppDomain.CurrentDomain.BaseDirectory, "network_log.txt");
    private static readonly string StatePath = Path.Combine(
        AppDomain.CurrentDomain.BaseDirectory, "network_state.json");

    private static void Log(string msg)
    {
        var line = $"[{DateTime.Now:HH:mm:ss.fff}] {msg}";
        try { File.AppendAllText(LogPath, line + Environment.NewLine); } catch { }
    }

    public static bool IsRunningAsAdmin()
    {
        using var identity = WindowsIdentity.GetCurrent();
        var principal = new WindowsPrincipal(identity);
        return principal.IsInRole(WindowsBuiltInRole.Administrator);
    }

    public static List<(string Name, string Description, List<string> IPs, int Index)> GetPhysicalNICs()
    {
        var results = new List<(string, string, List<string>, int)>();
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != OperationalStatus.Up) continue;
            if (nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
            if (nic.Description.Contains("Bluetooth", StringComparison.OrdinalIgnoreCase)) continue;
            if (nic.Description.Contains("Hyper-V", StringComparison.OrdinalIgnoreCase)) continue;
            if (nic.Description.Contains("Virtual", StringComparison.OrdinalIgnoreCase)) continue;

            var ips = new List<string>();
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
            {
                if (addr.Address.AddressFamily == AddressFamily.InterNetwork)
                    ips.Add(addr.Address.ToString());
            }

            if (ips.Count > 0)
            {
                var idx = nic.GetIPProperties().GetIPv4Properties()?.Index ?? 0;
                results.Add((nic.Name, nic.Description, ips, idx));
            }
        }
        return results;
    }

    public static bool IsTempIPExists()
    {
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != OperationalStatus.Up) continue;
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
            {
                if (addr.Address.ToString() == TempIP) return true;
            }
        }
        return false;
    }

    public static (bool success, string error) AddTempIPCustom(string nicName, string ip)
    {
        Log($"AddTempIPCustom: NIC='{nicName}', IP={ip}, IsAdmin={IsRunningAsAdmin()}");

        // Save original NIC state (DHCP/DNS) once per session for clean restore on exit
        SaveStateIfFirstTime(nicName);

        // Check if this IP already exists
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
            {
                if (addr.Address.ToString() == ip)
                {
                    Log("AddTempIPCustom: IP already exists, reusing");
                    return (true, "");
                }
            }
        }

        var (ok, output) = RunNetshWithOutput(
            $"interface ip add address \"{nicName}\" {ip} {TempMask}");
        if (ok || output.Contains("이미") || output.Contains("already"))
        {
            Log($"AddTempIPCustom: Success");
            return (true, "");
        }

        // Fallback: PowerShell
        var (ok2, output2) = RunPowerShellWithOutput(
            $"New-NetIPAddress -InterfaceAlias '{nicName}' -IPAddress {ip} -PrefixLength 24 -ErrorAction Stop");
        if (ok2 || output2.Contains("이미") || output2.Contains("already") || output2.Contains("5010"))
        {
            return (true, "");
        }

        return (false, $"임시 IP 추가 실패: {output}");
    }

    public static bool HasSubnet192168_1()
    {
        foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
        {
            if (nic.OperationalStatus != OperationalStatus.Up) continue;
            foreach (var addr in nic.GetIPProperties().UnicastAddresses)
            {
                if (addr.Address.AddressFamily == AddressFamily.InterNetwork &&
                    addr.Address.ToString().StartsWith("192.168.1."))
                    return true;
            }
        }
        return false;
    }

    public static (bool success, string error) AddTempIP(string nicName)
    {
        Log($"AddTempIP: NIC='{nicName}', IsAdmin={IsRunningAsAdmin()}");

        if (!IsRunningAsAdmin())
        {
            Log("AddTempIP: NOT running as admin!");
            return (false, "관리자 권한이 필요합니다.");
        }

        // Save original NIC state (DHCP/DNS) once per session for clean restore on exit
        SaveStateIfFirstTime(nicName);

        // Check if temp IP already exists (from previous run)
        if (IsTempIPExists())
        {
            Log("AddTempIP: Temp IP already exists, reusing");
            return (true, "");
        }

        // Try netsh with NIC name
        var (ok, output) = RunNetshWithOutput(
            $"interface ip add address \"{nicName}\" {TempIP} {TempMask}");

        if (ok)
        {
            Log($"AddTempIP: Success via NIC name");
            return (true, "");
        }

        Log($"AddTempIP: Failed via name: {output}");

        // "already exists" is success
        if (output.Contains("이미") || output.Contains("already") || output.Contains("5010"))
        {
            Log("AddTempIP: IP already exists, treating as success");
            return (true, "");
        }

        // Fallback: try PowerShell (handles Unicode NIC names better)
        var (ok2, output2) = RunPowerShellWithOutput(
            $"New-NetIPAddress -InterfaceAlias '{nicName}' -IPAddress {TempIP} -PrefixLength 24 -ErrorAction Stop");

        if (ok2)
        {
            Log($"AddTempIP: Success via PowerShell");
            return (true, "");
        }

        Log($"AddTempIP: Failed via PowerShell: {output2}");

        // Fallback 2: try netsh with interface index
        var nics = GetPhysicalNICs();
        var nic = nics.FirstOrDefault(n => n.Name == nicName);
        if (nic.Index > 0)
        {
            var (ok3, output3) = RunNetshWithOutput(
                $"interface ip add address {nic.Index} {TempIP} {TempMask}");
            if (ok3)
            {
                Log($"AddTempIP: Success via index {nic.Index}");
                return (true, "");
            }
            Log($"AddTempIP: Failed via index: {output3}");
        }

        return (false, $"임시 IP 추가 실패.\n\n상세: {output}\n{output2}");
    }

    public static bool RemoveTempIP(string nicName)
    {
        Log($"RemoveTempIP: NIC='{nicName}'");

        // Remove all known temp IPs (192.168.x.100 patterns)
        // Use PowerShell to find and remove non-primary IPs we added
        RunPowerShellWithOutput(
            $"Get-NetIPAddress -InterfaceAlias '{nicName}' -AddressFamily IPv4 -ErrorAction SilentlyContinue | " +
            $"Where-Object {{ $_.IPAddress -like '*.100' -and $_.PrefixOrigin -eq 'Manual' }} | " +
            $"Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue");

        // Also try removing the default temp IP directly
        RunNetshWithOutput($"interface ip delete address \"{nicName}\" {TempIP}");

        // Restore original DHCP/DNS state (only if we recorded the NIC was DHCP before)
        var state = LoadNicState();
        if (state != null && state.NicName == nicName)
        {
            RestoreNicState(state);
            ClearNicState();
        }

        return true; // Best effort
    }

    // ─── NIC state save/restore (DHCP/DNS) ──────────────────────────────────

    public static NicState CaptureNicState(string nicName)
    {
        // Get-NetIPInterface.Dhcp returns "Enabled" or "Disabled"
        var (_, dhcpOut) = RunPowerShellWithOutput(
            $"(Get-NetIPInterface -InterfaceAlias '{nicName}' -AddressFamily IPv4 -ErrorAction SilentlyContinue).Dhcp");
        bool wasDhcp = dhcpOut.Trim().Equals("Enabled", StringComparison.OrdinalIgnoreCase);

        // DNS auto-config: ServerAddresses array empty means DHCP-supplied DNS
        var (_, dnsOut) = RunPowerShellWithOutput(
            $"$s = (Get-DnsClientServerAddress -InterfaceAlias '{nicName}' -AddressFamily IPv4 -ErrorAction SilentlyContinue).ServerAddresses; " +
            $"if ($s) {{ $s.Count }} else {{ 0 }}");
        bool wasDnsAuto = dnsOut.Trim() == "0";

        var state = new NicState
        {
            NicName = nicName,
            WasDhcp = wasDhcp,
            WasDnsAuto = wasDnsAuto,
            SavedAt = DateTime.Now
        };
        Log($"CaptureNicState: NIC='{nicName}', WasDhcp={wasDhcp}, WasDnsAuto={wasDnsAuto}");
        return state;
    }

    private static void SaveStateIfFirstTime(string nicName)
    {
        // Only capture state once per session — subsequent AddTempIP calls reuse it.
        if (File.Exists(StatePath))
        {
            Log($"SaveStateIfFirstTime: state file exists, keeping original capture");
            return;
        }
        try
        {
            var state = CaptureNicState(nicName);
            File.WriteAllText(StatePath, JsonSerializer.Serialize(state));
            Log($"SaveStateIfFirstTime: state saved to {StatePath}");
        }
        catch (Exception ex)
        {
            Log($"SaveStateIfFirstTime: error {ex.Message}");
        }
    }

    public static NicState? LoadNicState()
    {
        try
        {
            if (!File.Exists(StatePath)) return null;
            var json = File.ReadAllText(StatePath);
            return JsonSerializer.Deserialize<NicState>(json);
        }
        catch (Exception ex)
        {
            Log($"LoadNicState: error {ex.Message}");
            return null;
        }
    }

    public static void ClearNicState()
    {
        try
        {
            if (File.Exists(StatePath)) File.Delete(StatePath);
            Log("ClearNicState: state file deleted");
        }
        catch (Exception ex) { Log($"ClearNicState: error {ex.Message}"); }
    }

    public static void RestoreNicState(NicState state)
    {
        Log($"RestoreNicState: NIC='{state.NicName}', WasDhcp={state.WasDhcp}, WasDnsAuto={state.WasDnsAuto}");
        if (state.WasDhcp)
        {
            var (ok, output) = RunNetshWithOutput(
                $"interface ipv4 set address \"{state.NicName}\" source=dhcp");
            Log($"  Restore DHCP: ok={ok}, output={output}");
            if (!ok)
            {
                // Fallback: PowerShell
                RunPowerShellWithOutput(
                    $"Set-NetIPInterface -InterfaceAlias '{state.NicName}' -Dhcp Enabled -ErrorAction SilentlyContinue");
            }
        }
        if (state.WasDnsAuto)
        {
            var (ok, output) = RunNetshWithOutput(
                $"interface ipv4 set dnsservers \"{state.NicName}\" source=dhcp");
            Log($"  Restore DNS auto: ok={ok}, output={output}");
            if (!ok)
            {
                RunPowerShellWithOutput(
                    $"Set-DnsClientServerAddress -InterfaceAlias '{state.NicName}' -ResetServerAddresses -ErrorAction SilentlyContinue");
            }
        }
    }

    private static (bool success, string output) RunNetshWithOutput(string args)
    {
        try
        {
            var psi = new ProcessStartInfo("netsh", args)
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            var proc = Process.Start(psi)!;
            var stdout = proc.StandardOutput.ReadToEnd();
            var stderr = proc.StandardError.ReadToEnd();
            proc.WaitForExit(10000);

            var output = (stdout + " " + stderr).Trim();
            Log($"netsh '{args}' → exit={proc.ExitCode}, output={output}");
            return (proc.ExitCode == 0, output);
        }
        catch (Exception ex)
        {
            Log($"netsh exception: {ex.Message}");
            return (false, ex.Message);
        }
    }

    private static (bool success, string output) RunPowerShellWithOutput(string command)
    {
        try
        {
            var psi = new ProcessStartInfo("powershell", $"-NoProfile -Command \"{command}\"")
            {
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };
            var proc = Process.Start(psi)!;
            var stdout = proc.StandardOutput.ReadToEnd();
            var stderr = proc.StandardError.ReadToEnd();
            proc.WaitForExit(10000);

            var output = (stdout + " " + stderr).Trim();
            Log($"powershell '{command}' → exit={proc.ExitCode}, output={output}");
            return (proc.ExitCode == 0, output);
        }
        catch (Exception ex)
        {
            Log($"powershell exception: {ex.Message}");
            return (false, ex.Message);
        }
    }
}
