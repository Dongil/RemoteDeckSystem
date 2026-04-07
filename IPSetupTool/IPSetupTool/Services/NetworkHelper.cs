using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Security.Principal;

namespace IPSetupTool.Services;

public static class NetworkHelper
{
    private const string TempIP = "192.168.1.100";
    private const string TempMask = "255.255.255.0";

    private static readonly string LogPath = Path.Combine(
        AppDomain.CurrentDomain.BaseDirectory, "network_log.txt");

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

        return true; // Best effort
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
