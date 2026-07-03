// Design Ref: §3.1 — Persisted device entry. Password stored as DPAPI base64, never plaintext.
// [Browsable(false)] on every property prevents DataGridView/BindingList from auto-generating
// columns that would leak internal state (Id, AuthPasswordProtected, ...) to the UI.
// Explicit columns in MainForm.Designer.cs use DataPropertyName which is unaffected by this.
using System.ComponentModel;
using System.Text.Json.Serialization;

namespace IntegrateController.Models;

public sealed class DeviceEntry
{
    [Browsable(false)]
    public string Id { get; set; } = Guid.NewGuid().ToString("N");

    [Browsable(false)]
    public string Ip { get; set; } = "";

    [Browsable(false)]
    public int Port { get; set; } = 5050;

    [Browsable(false)]
    public string AuthUser { get; set; } = "admin";

    // Plan SC: PW must not appear plaintext in devices.json. Base64 of DPAPI-protected bytes.
    [Browsable(false)]
    public string AuthPasswordProtected { get; set; } = "";

    [Browsable(false)]
    public int Order { get; set; }

    [Browsable(false)]
    public int PollIntervalSec { get; set; } = 3;

    [Browsable(false)]
    public int TimeoutMs { get; set; } = 2000;

    [Browsable(false)]
    [JsonIgnore]
    public string BaseUrl => $"http://{Ip}:{Port}";
}
