// Design Ref: §3.2, §7 — DPAPI ProtectedData (CurrentUser + app entropy). devices.json under %LOCALAPPDATA%.
// Plan SC: PW must never appear plaintext on disk.
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using IntegrateController.Models;

namespace IntegrateController.Services;

public sealed class DeviceStore
{
    // App-specific entropy. Different app/scope cannot decrypt even under same Windows account.
    private static readonly byte[] Entropy = new byte[]
    {
        0x52, 0x44, 0x53, 0x49, 0x43, 0x32, 0x30, 0x32, 0x36,
        0x69, 0x6E, 0x74, 0x65, 0x67, 0x72, 0x61, 0x74, 0x65
    };

    private static readonly JsonSerializerOptions JsonOpts = new()
    {
        WriteIndented = true
    };

    public string FilePath { get; }

    public DeviceStore(string? overridePath = null)
    {
        if (overridePath != null)
        {
            FilePath = overridePath;
        }
        else
        {
            var dir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "IntegrateController");
            Directory.CreateDirectory(dir);
            FilePath = Path.Combine(dir, "devices.json");
        }
    }

    public DeviceList Load()
    {
        if (!File.Exists(FilePath)) return new DeviceList();

        try
        {
            var json = File.ReadAllText(FilePath, Encoding.UTF8);
            var dto = JsonSerializer.Deserialize<StoreDto>(json, JsonOpts);
            if (dto?.Devices == null) return new DeviceList();

            var ordered = dto.Devices.OrderBy(d => d.Order).ToList();
            return new DeviceList(ordered);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Trace.WriteLine($"[DeviceStore] Load failed: {ex.Message}");
            return new DeviceList();
        }
    }

    public void Save(DeviceList devices)
    {
        devices.ReassignOrder();
        var dto = new StoreDto
        {
            Version = 1,
            Devices = devices.ToList()
        };
        var json = JsonSerializer.Serialize(dto, JsonOpts);

        var tmp = FilePath + ".tmp";
        File.WriteAllText(tmp, json, Encoding.UTF8);
        if (File.Exists(FilePath)) File.Replace(tmp, FilePath, null);
        else File.Move(tmp, FilePath);
    }

    // ─── DPAPI helpers ────────────────────────────────────────────

    public static string ProtectPassword(string plain)
    {
        if (string.IsNullOrEmpty(plain)) return "";
        try
        {
            var data = Encoding.UTF8.GetBytes(plain);
            var enc = ProtectedData.Protect(data, Entropy, DataProtectionScope.CurrentUser);
            return Convert.ToBase64String(enc);
        }
        catch (Exception ex)
        {
            System.Diagnostics.Trace.WriteLine($"[DeviceStore] Protect failed: {ex.Message}");
            return "";
        }
    }

    public static string? UnprotectPassword(string protectedBase64)
    {
        if (string.IsNullOrEmpty(protectedBase64)) return null;
        try
        {
            var enc = Convert.FromBase64String(protectedBase64);
            var dec = ProtectedData.Unprotect(enc, Entropy, DataProtectionScope.CurrentUser);
            return Encoding.UTF8.GetString(dec);
        }
        catch (Exception ex)
        {
            // Different user/PC or corrupted blob.
            System.Diagnostics.Trace.WriteLine($"[DeviceStore] Unprotect failed: {ex.Message}");
            return null;
        }
    }

    // ─── DTO ──────────────────────────────────────────────────────

    private sealed class StoreDto
    {
        public int Version { get; set; } = 1;
        public List<DeviceEntry> Devices { get; set; } = new();
    }
}
