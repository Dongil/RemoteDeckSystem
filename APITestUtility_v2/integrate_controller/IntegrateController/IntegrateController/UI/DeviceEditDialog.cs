// Design Ref: §5.4 — Add/Edit device modal with IP regex validation + ErrorProvider.
// Label field removed (UI shows device_name from /api/status + device_id from /api/config).
using System.Text.RegularExpressions;
using IntegrateController.Models;
using IntegrateController.Services;

namespace IntegrateController.UI;

public partial class DeviceEditDialog : Form
{
    private static readonly Regex IpRegex = new(@"^\d{1,3}(\.\d{1,3}){3}$", RegexOptions.Compiled);

    public DeviceEntry Result { get; private set; } = new();

    // Parameterless constructor required by VS WinForms Designer.
    public DeviceEditDialog() : this(null) { }

    public DeviceEditDialog(DeviceEntry? existing)
    {
        InitializeComponent();

        if (existing != null)
        {
            Text = "Edit Device";
            txtIp.Text = existing.Ip;
            numPort.Value = existing.Port;
            txtUser.Text = existing.AuthUser;
            var pw = DeviceStore.UnprotectPassword(existing.AuthPasswordProtected) ?? "";
            txtPassword.Text = pw;
            numInterval.Value = Math.Clamp(existing.PollIntervalSec, 1, 30);
            numTimeout.Value = Math.Clamp(existing.TimeoutMs, 500, 10000);
            Result = existing;
        }
        else
        {
            Text = "Add Device";
            numPort.Value = 5050;
            txtUser.Text = "admin";
            numInterval.Value = 3;
            numTimeout.Value = 2000;
            Result = new DeviceEntry();
        }

        btnOk.Click += BtnOk_Click;
        btnCancel.Click += (_, _) => { DialogResult = DialogResult.Cancel; Close(); };
        AcceptButton = btnOk;
        CancelButton = btnCancel;
    }

    private void BtnOk_Click(object? sender, EventArgs e)
    {
        errorProvider.Clear();
        bool ok = true;

        if (!IpRegex.IsMatch(txtIp.Text.Trim()))
        {
            errorProvider.SetError(txtIp, "Invalid IPv4 address");
            ok = false;
        }
        else
        {
            foreach (var p in txtIp.Text.Trim().Split('.'))
            {
                if (!int.TryParse(p, out var n) || n < 0 || n > 255)
                {
                    errorProvider.SetError(txtIp, "Each octet must be 0-255");
                    ok = false;
                    break;
                }
            }
        }
        if (string.IsNullOrEmpty(txtUser.Text))
        {
            errorProvider.SetError(txtUser, "User is required");
            ok = false;
        }

        if (!ok) return;

        Result.Ip = txtIp.Text.Trim();
        Result.Port = (int)numPort.Value;
        Result.AuthUser = txtUser.Text;
        Result.AuthPasswordProtected = DeviceStore.ProtectPassword(txtPassword.Text);
        Result.PollIntervalSec = (int)numInterval.Value;
        Result.TimeoutMs = (int)numTimeout.Value;

        DialogResult = DialogResult.OK;
        Close();
    }
}
