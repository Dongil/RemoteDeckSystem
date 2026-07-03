// Design Ref: §2.1, §5 — Main window. Composes DeviceStore + RemoteDeckClient + DevicePoller + DataGridView.
// All Poller.StatusUpdated events are marshalled onto UI thread via Invoke.
using System.ComponentModel;
using IntegrateController.Models;
using IntegrateController.Services;

namespace IntegrateController.UI;

public partial class MainForm : Form
{
    private readonly DeviceStore _store = new();
    private readonly RemoteDeckClient _client = new();
    private readonly DevicePoller _poller;
    // RemoteDeck_PC_v2.5 §5.5 — Log poller mirrors DevicePoller pattern.
    private readonly LogPoller _logPoller;
    private ListView _logList = null!;
    private ColumnHeader _colLogTime = null!;
    private ColumnHeader _colLogEvent = null!;
    private ColumnHeader _colLogDetail = null!;

    private DeviceList _devices = new();
    // IP column is added programmatically (NOT via VS Designer) — Designer keeps dropping
    // the column entry when InitializeComponent is regenerated. Keeping it in code lets
    // the layout survive any number of Designer regenerations.
    private DataGridViewTextBoxColumn _colIp = null!;
    private readonly Dictionary<string, DeviceStatus> _statusById =
        new(StringComparer.Ordinal);
    private readonly Dictionary<string, bool> _authIssueById =
        new(StringComparer.Ordinal);

    public MainForm()
    {
        InitializeComponent();
        EnableGridDoubleBuffer();

        // Defense-in-depth: VS Designer regeneration drops AutoGenerateColumns when it
        // equals the default value (true). Re-asserting it here keeps the unbound layout
        // even after the Designer rewrites InitializeComponent.
        grid.AutoGenerateColumns = false;

        // IP column added in code (never via Designer) so it survives regeneration.
        AddIpColumn();

        _poller = new DevicePoller(_client);
        _poller.StatusUpdated += Poller_StatusUpdated;

        // RemoteDeck_PC_v2.5 §5.5 — Log poller + inline log ListView added by code (not Designer)
        // so the layout survives any Designer regeneration ([[project_winforms_designer]]).
        _logPoller = new LogPoller(_client);
        _logPoller.LogsUpdated += LogPoller_LogsUpdated;
        AddLogView();

        Load += MainForm_Load;
        FormClosing += MainForm_FormClosing;

        btnAdd.Click += BtnAdd_Click;
        btnEdit.Click += BtnEdit_Click;
        btnDelete.Click += BtnDelete_Click;
        btnUp.Click += BtnUp_Click;
        btnDown.Click += BtnDown_Click;
        btnReboot.Click += BtnReboot_Click;

        grid.SelectionChanged += Grid_SelectionChanged;
        grid.CellDoubleClick += (_, _) => BtnEdit_Click(null, EventArgs.Empty);
        grid.MouseUp += Grid_MouseUp;
        grid.DataBindingComplete += (_, _) => RefreshAllUnboundCells();

        ctxMenuOpenBrowser.Click += CtxMenuOpenBrowser_Click;
        ctxMenuReboot.Click += BtnReboot_Click;
        ctxMenuEdit.Click += BtnEdit_Click;
        ctxMenuDelete.Click += BtnDelete_Click;

        clockTimer.Tick += (_, _) =>
            statusClock.Text = DateTime.Now.ToString("HH:mm:ss");
    }

    // ─── Lifecycle ────────────────────────────────────────────────

    private void MainForm_Load(object? sender, EventArgs e)
    {
        _devices = _store.Load();
        _devices.ListChanged += Devices_ListChanged;
        grid.DataSource = _devices;

        // Detect DPAPI failures on every loaded device.
        foreach (var d in _devices)
        {
            if (!string.IsNullOrEmpty(d.AuthPasswordProtected) &&
                DeviceStore.UnprotectPassword(d.AuthPasswordProtected) == null)
            {
                _authIssueById[d.Id] = true;
            }
        }

        int idx = 0;
        foreach (var d in _devices)
        {
            _poller.Start(d);
            // Design §5.5 stagger — 300ms per device offset so 14대 폴링이 5s 안에 균등 분산됨
            _logPoller.Start(d, idx++);
        }

        UpdateStatusMessage(_devices.Count == 0
            ? "단말이 없습니다. [+ Add] 버튼으로 추가하세요."
            : $"{_devices.Count}대 폴링 시작.");
        clockTimer.Start();
        RefreshAllUnboundCells();
        RefreshDetailPanel();
    }

    private void MainForm_FormClosing(object? sender, FormClosingEventArgs e)
    {
        clockTimer.Stop();
        _poller.StopAll();
        _logPoller.StopAll();
        try { _store.Save(_devices); }
        catch (Exception ex)
        {
            MessageBox.Show("저장 실패: " + ex.Message, "IntegrateController",
                MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    // ─── Toolbar handlers ─────────────────────────────────────────

    private void BtnAdd_Click(object? sender, EventArgs e)
    {
        using var dlg = new DeviceEditDialog();
        if (dlg.ShowDialog(this) != DialogResult.OK) return;
        _devices.Add(dlg.Result);
        _devices.ReassignOrder();
        SaveQuiet();
        _poller.Start(dlg.Result);
        _logPoller.Start(dlg.Result, _devices.Count - 1);
        UpdateStatusMessage($"단말 추가: {DisplayName(dlg.Result)}");
    }

    private void BtnEdit_Click(object? sender, EventArgs e)
    {
        var device = SelectedDevice();
        if (device == null) return;

        using var dlg = new DeviceEditDialog(device);
        if (dlg.ShowDialog(this) != DialogResult.OK) return;

        _authIssueById.Remove(device.Id);
        _poller.InvalidateDeviceIdCache(device.Id);

        SaveQuiet();
        _poller.Restart(device);
        // Restart log polling — device auth/IP may have changed.
        _logPoller.Stop(device.Id);
        _logPoller.Start(device, 0);
        grid.Refresh();
        UpdateStatusMessage($"단말 편집: {DisplayName(device)}");
    }

    private void BtnDelete_Click(object? sender, EventArgs e)
    {
        var device = SelectedDevice();
        if (device == null) return;

        var name = DisplayName(device);
        var res = MessageBox.Show($"단말 '{name}' ({device.Ip}) 을(를) 삭제할까요?",
            "삭제 확인", MessageBoxButtons.YesNo, MessageBoxIcon.Question);
        if (res != DialogResult.Yes) return;

        _poller.Stop(device.Id);
        _logPoller.Stop(device.Id);
        _statusById.Remove(device.Id);
        _authIssueById.Remove(device.Id);
        _devices.Remove(device);
        _devices.ReassignOrder();
        SaveQuiet();
        UpdateStatusMessage($"단말 삭제: {name}");
        RefreshDetailPanel();
    }

    private void BtnUp_Click(object? sender, EventArgs e)
    {
        var idx = grid.CurrentRow?.Index ?? -1;
        if (idx <= 0) return;
        _devices.MoveUp(idx);
        SaveQuiet();
        grid.ClearSelection();
        grid.Rows[idx - 1].Selected = true;
    }

    private void BtnDown_Click(object? sender, EventArgs e)
    {
        var idx = grid.CurrentRow?.Index ?? -1;
        if (idx < 0 || idx >= _devices.Count - 1) return;
        _devices.MoveDown(idx);
        SaveQuiet();
        grid.ClearSelection();
        grid.Rows[idx + 1].Selected = true;
    }

    private void CtxMenuOpenBrowser_Click(object? sender, EventArgs e)
    {
        var device = SelectedDevice();
        if (device == null) return;

        // Plain URL only — the browser will pop its own Basic Auth dialog and the
        // operator types the credentials manually. Avoids the XHR auth-cache timing
        // issue caused by URL-embedded credentials.
        var url = device.BaseUrl;
        try
        {
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
            {
                FileName = url,
                UseShellExecute = true,
            });
            UpdateStatusMessage($"브라우저 열기: {url}");
        }
        catch (Exception ex)
        {
            UpdateStatusMessage($"브라우저 열기 실패: {ex.Message}");
            MessageBox.Show($"브라우저를 열 수 없습니다.\n{url}\n\n{ex.Message}",
                "IntegrateController", MessageBoxButtons.OK, MessageBoxIcon.Warning);
        }
    }

    private async void BtnReboot_Click(object? sender, EventArgs e)
    {
        var device = SelectedDevice();
        if (device == null) return;

        var name = DisplayName(device);
        var res = MessageBox.Show($"단말 '{name}' ({device.Ip}) 재부팅?",
            "재부팅 확인", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
        if (res != DialogResult.Yes) return;

        UpdateStatusMessage($"재부팅 요청 중: {name}...");
        var sw = System.Diagnostics.Stopwatch.StartNew();
        var result = await _client.RebootAsync(device, CancellationToken.None).ConfigureAwait(true);
        sw.Stop();

        if (result.Ok)
        {
            // Error field may contain an informational hint when reboot was inferred
            // from timeout/connection-reset (firmware restarts before flushing response).
            var hint = string.IsNullOrEmpty(result.Error) ? "" : $" — {result.Error}";
            UpdateStatusMessage(
                $"재부팅 요청 전송됨: {name} (RTT {sw.ElapsedMilliseconds}ms){hint}");
        }
        else
        {
            UpdateStatusMessage(
                $"재부팅 실패: {name} — {result.Error}");
            MessageBox.Show($"재부팅 실패: {result.Error}", "IntegrateController",
                MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    // ─── Grid / Poller ────────────────────────────────────────────

    private void Devices_ListChanged(object? sender, ListChangedEventArgs e)
    {
        // No., status, device_name, device_id columns are unbound — refresh after every
        // add/remove/move so row numbering stays in sync.
        RefreshAllUnboundCells();
    }

    private void Poller_StatusUpdated(object? sender, DeviceStatusUpdatedEventArgs e)
    {
        if (IsDisposed) return;
        if (InvokeRequired)
        {
            try { BeginInvoke(new Action(() => Poller_StatusUpdated(sender, e))); }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
            return;
        }

        _statusById[e.DeviceId] = e.Status;

        for (int i = 0; i < _devices.Count; i++)
        {
            if (!string.Equals(_devices[i].Id, e.DeviceId, StringComparison.Ordinal)) continue;
            if (i >= grid.Rows.Count) break;

            ApplyStatusToRow(i, _devices[i], e.Status);

            if (e.Status.LastError != null && !e.Status.Online &&
                (e.Status.LastError.Contains("인증") || e.Status.LastError.Contains("401")))
            {
                UpdateStatusMessage($"인증 실패: {DisplayName(_devices[i])}");
            }
            break;
        }

        if (grid.CurrentRow?.Index is int sel && sel >= 0 && sel < _devices.Count &&
            string.Equals(_devices[sel].Id, e.DeviceId, StringComparison.Ordinal))
        {
            RefreshDetailPanel();
        }
    }

    private void Grid_SelectionChanged(object? sender, EventArgs e)
    {
        RefreshDetailPanel();
        RefreshLogView();
    }

    private void LogPoller_LogsUpdated(object? sender, LogsUpdatedEventArgs e)
    {
        if (IsDisposed) return;
        if (InvokeRequired)
        {
            try { BeginInvoke(new Action(() => LogPoller_LogsUpdated(sender, e))); }
            catch (ObjectDisposedException) { }
            catch (InvalidOperationException) { }
            return;
        }
        // Only refresh view when the updated device is the currently selected one.
        var sel = SelectedDevice();
        if (sel != null && string.Equals(sel.Id, e.DeviceId, StringComparison.Ordinal))
            RefreshLogView();
    }

    private void RefreshLogView()
    {
        var d = SelectedDevice();
        _logList.BeginUpdate();
        try
        {
            _logList.Items.Clear();
            if (d == null) return;
            var logs = _logPoller.GetLogs(d.Id, 100);   // Design §12: 표시 100건
            foreach (var l in logs)
            {
                var item = new ListViewItem(string.IsNullOrEmpty(l.TimeStr) ? l.Timestamp.ToString() : l.TimeStr);
                item.SubItems.Add(l.EventStr);
                item.SubItems.Add(l.Detail);
                _logList.Items.Add(item);
            }
        }
        finally { _logList.EndUpdate(); }
    }

    private void Grid_MouseUp(object? sender, MouseEventArgs e)
    {
        if (e.Button != MouseButtons.Right) return;
        var hit = grid.HitTest(e.X, e.Y);
        if (hit.RowIndex < 0 || hit.RowIndex >= _devices.Count) return;
        grid.ClearSelection();
        grid.Rows[hit.RowIndex].Selected = true;
        // Use Cells[0] (ColNo) — always present. Avoids dependency on any single column
        // that VS Designer might drop in a regenerate.
        if (grid.Rows[hit.RowIndex].Cells.Count > 0)
            grid.CurrentCell = grid.Rows[hit.RowIndex].Cells[0];
        ctxMenuGrid.Show(grid, e.X, e.Y);
    }

    // ─── Unbound cell rendering ───────────────────────────────────

    private void RefreshAllUnboundCells()
    {
        int upper = Math.Min(_devices.Count, grid.Rows.Count);
        for (int i = 0; i < upper; i++)
        {
            var d = _devices[i];
            grid.Rows[i].Cells[ColNo.Index].Value = (i + 1).ToString();
            if (_statusById.TryGetValue(d.Id, out var status))
            {
                ApplyStatusToRow(i, d, status);
            }
            else
            {
                var row = grid.Rows[i];
                bool authIssue = _authIssueById.GetValueOrDefault(d.Id, false);
                StyleRowBackground(row, null, authIssue);
                StyleStatusCell(row, null);
                row.Cells[ColDeviceName.Index].Value = "-";
                row.Cells[ColDeviceId.Index].Value = "-";
                row.Cells[_colIp.Index].Value = d.Ip;
                row.Cells[ColPc.Index].Value = StatusFormatter.FormatPc(null);
                row.Cells[ColGpio.Index].Value = StatusFormatter.FormatGpio(null);
                row.Cells[ColFw.Index].Value = StatusFormatter.FormatFw(null);
                row.Cells[ColUptime.Index].Value = StatusFormatter.FormatUptime(null);
                row.Cells[ColLastSeen.Index].Value = StatusFormatter.FormatLastSeen(null);
            }
        }
    }

    private void ApplyStatusToRow(int rowIndex, DeviceEntry d, DeviceStatus status)
    {
        var row = grid.Rows[rowIndex];
        bool authIssue = _authIssueById.GetValueOrDefault(d.Id, false);

        StyleRowBackground(row, status, authIssue);
        StyleStatusCell(row, status);

        row.Cells[ColNo.Index].Value = (rowIndex + 1).ToString();
        row.Cells[ColDeviceName.Index].Value = string.IsNullOrEmpty(status.DeviceName) ? "-" : status.DeviceName;
        row.Cells[ColDeviceId.Index].Value = string.IsNullOrEmpty(status.DeviceId) ? "-" : status.DeviceId;
        row.Cells[_colIp.Index].Value = d.Ip;
        row.Cells[ColPc.Index].Value = StatusFormatter.FormatPc(status);
        row.Cells[ColGpio.Index].Value = StatusFormatter.FormatGpio(status);
        row.Cells[ColFw.Index].Value = StatusFormatter.FormatFw(status);
        row.Cells[ColUptime.Index].Value = StatusFormatter.FormatUptime(status);
        row.Cells[ColLastSeen.Index].Value = StatusFormatter.FormatLastSeen(status);
    }

    // Selection BG/FG colors keep the row's online/offline tint visible when selected.
    // Default DataGridView selection paints with SystemColors.Highlight (blue), which
    // wipes our row.DefaultCellStyle.BackColor and forces SelectionForeColor to white
    // (erasing the status dot color).
    private static void StyleRowBackground(DataGridViewRow row, DeviceStatus? status, bool authIssue)
    {
        row.DefaultCellStyle.BackColor = StatusFormatter.RowBackColor(status, authIssue);
        row.DefaultCellStyle.SelectionBackColor = StatusFormatter.RowSelectionBackColor(status, authIssue);
        row.DefaultCellStyle.SelectionForeColor = System.Drawing.Color.Black;
    }

    private void StyleStatusCell(DataGridViewRow row, DeviceStatus? status)
    {
        var cell = row.Cells[ColStatus.Index];
        var fg = StatusFormatter.StatusColor(status);
        cell.Value = StatusFormatter.StatusGlyph(status);
        cell.Style.ForeColor = fg;
        // Preserve the green/red dot color even when the row is selected.
        cell.Style.SelectionForeColor = fg;
    }

    // ─── Detail panel ─────────────────────────────────────────────

    private void RefreshDetailPanel()
    {
        var idx = grid.CurrentRow?.Index ?? -1;
        if (idx < 0 || idx >= _devices.Count)
        {
            txtDetail.Text = "";
            return;
        }
        var d = _devices[idx];
        var header = $"[{DisplayName(d)}] {d.Ip}:{d.Port} user={d.AuthUser}";
        bool authIssue = _authIssueById.GetValueOrDefault(d.Id, false);
        if (authIssue)
        {
            txtDetail.Text = $"{header}\r\n⚠ DPAPI 복호화 실패: 다른 Windows 계정/PC에서 저장됨. 단말 편집으로 비밀번호 재입력 필요.";
            return;
        }
        if (!_statusById.TryGetValue(d.Id, out var st))
        {
            txtDetail.Text = $"{header}\r\n(아직 폴링 결과 없음)";
            return;
        }

        var lines = new System.Text.StringBuilder();
        lines.AppendLine(header);
        lines.AppendLine($"online={st.Online}  pc_on={st.PcOn}  relay1={st.Relay1}  relay2={st.Relay2}");
        lines.AppendLine($"gpio={st.GpioString}  uptime={st.UptimeFormatted}");
        lines.AppendLine($"net_mode={st.NetMode}  fw_ver={st.FwVer}  device_name={st.DeviceName}  device_id={st.DeviceId}");
        lines.AppendLine($"mqtt_connected={st.MqttConnected}  ntp_synced={st.NtpSynced}  time={st.Time}");
        lines.AppendLine($"heap_free={st.HeapFree}  heap_min={st.HeapMin}");
        lines.AppendLine($"failures={st.ConsecutiveFailures}  last_seen={st.LastSeen:yyyy-MM-dd HH:mm:ss}");
        if (st.LastError != null) lines.AppendLine($"last_error={st.LastError}");
        if (st.RawJson != null)
        {
            lines.AppendLine();
            lines.AppendLine("raw:");
            lines.AppendLine(st.RawJson);
        }
        txtDetail.Text = lines.ToString();
    }

    // ─── Helpers ──────────────────────────────────────────────────

    private DeviceEntry? SelectedDevice()
    {
        var idx = grid.CurrentRow?.Index ?? -1;
        if (idx < 0 || idx >= _devices.Count) return null;
        return _devices[idx];
    }

    // Label property was removed (per Plan FR). Use polled device_name when available,
    // falling back to IP so log/dialog text is always meaningful.
    private string DisplayName(DeviceEntry d)
    {
        if (_statusById.TryGetValue(d.Id, out var s) && !string.IsNullOrEmpty(s.DeviceName))
            return s.DeviceName;
        return d.Ip;
    }

    private void SaveQuiet()
    {
        try { _store.Save(_devices); }
        catch (Exception ex)
        {
            UpdateStatusMessage("저장 실패: " + ex.Message);
        }
    }

    private void UpdateStatusMessage(string text)
    {
        statusMessage.Text = text;
    }

    private void AddIpColumn()
    {
        _colIp = new DataGridViewTextBoxColumn
        {
            Name = "ColIp",
            HeaderText = "IP",
            // No DataPropertyName — [Browsable(false)] on DeviceEntry.Ip would hide it
            // from BindingList.GetItemProperties() and the cell would stay empty.
            // Populated by ApplyStatusToRow / RefreshAllUnboundCells instead.
            FillWeight = 22F,
            ReadOnly = true,
        };
        // Insert after ColDeviceId so layout matches Plan: No / 연결 / 이름 / ID / IP / PC / ...
        int insertAt = grid.Columns.Count;
        if (ColDeviceId != null)
        {
            int idx = grid.Columns.IndexOf(ColDeviceId);
            if (idx >= 0) insertAt = idx + 1;
        }
        grid.Columns.Insert(insertAt, _colIp);
    }

    private void EnableGridDoubleBuffer()
    {
        var prop = typeof(System.Windows.Forms.DataGridView).GetProperty(
            "DoubleBuffered",
            System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
        prop?.SetValue(grid, true, null);
    }

    // Design Ref: §5.5 — Adds log ListView below existing detail TextBox.
    // Panel2 originally hosts txtDetail (Dock=Fill). We wrap it in a horizontal SplitContainer
    // so txtDetail keeps its size and the log list occupies the lower half.
    // Kept entirely in code (never in Designer) so VS regeneration cannot drop it.
    private void AddLogView()
    {
        _colLogTime   = new ColumnHeader { Text = "시간", Width = 130 };
        _colLogEvent  = new ColumnHeader { Text = "이벤트", Width = 80 };
        _colLogDetail = new ColumnHeader { Text = "상세", Width = 500 };

        _logList = new ListView
        {
            Dock = DockStyle.Fill,
            View = View.Details,
            FullRowSelect = true,
            GridLines = true,
            HeaderStyle = ColumnHeaderStyle.Nonclickable,
            Font = new System.Drawing.Font("Consolas", 9F),
        };
        _logList.Columns.AddRange(new[] { _colLogTime, _colLogEvent, _colLogDetail });

        var detailSplit = new SplitContainer
        {
            Dock = DockStyle.Fill,
            Orientation = Orientation.Horizontal,
            SplitterDistance = 120,   // upper: detail text
        };

        // Re-parent existing txtDetail into the upper panel.
        splitMain.Panel2.Controls.Remove(txtDetail);
        detailSplit.Panel1.Controls.Add(txtDetail);
        detailSplit.Panel2.Controls.Add(_logList);
        splitMain.Panel2.Controls.Add(detailSplit);
    }
}
