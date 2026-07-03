#nullable disable

namespace IntegrateController.UI;

partial class MainForm
{
    /// <summary>
    ///  Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    ///  Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
        if (disposing && (components != null))
        {
            components.Dispose();
        }
        base.Dispose(disposing);
    }

	#region Windows Form Designer generated code

	/// <summary>
	///  Required method for Designer support - do not modify
	///  the contents of this method with the code editor.
	/// </summary>
	private void InitializeComponent()
	{
		components = new System.ComponentModel.Container();
		DataGridViewCellStyle dataGridViewCellStyle1 = new DataGridViewCellStyle();
		DataGridViewCellStyle dataGridViewCellStyle2 = new DataGridViewCellStyle();
		toolStrip = new ToolStrip();
		btnAdd = new ToolStripButton();
		btnEdit = new ToolStripButton();
		btnDelete = new ToolStripButton();
		sep1 = new ToolStripSeparator();
		btnUp = new ToolStripButton();
		btnDown = new ToolStripButton();
		sep2 = new ToolStripSeparator();
		btnReboot = new ToolStripButton();
		splitMain = new SplitContainer();
		grid = new DataGridView();
		ColNo = new DataGridViewTextBoxColumn();
		ColStatus = new DataGridViewTextBoxColumn();
		ColDeviceName = new DataGridViewTextBoxColumn();
		ColDeviceId = new DataGridViewTextBoxColumn();
		ColPc = new DataGridViewTextBoxColumn();
		ColGpio = new DataGridViewTextBoxColumn();
		ColFw = new DataGridViewTextBoxColumn();
		ColUptime = new DataGridViewTextBoxColumn();
		ColLastSeen = new DataGridViewTextBoxColumn();
		txtDetail = new TextBox();
		ctxMenuGrid = new ContextMenuStrip(components);
		ctxMenuOpenBrowser = new ToolStripMenuItem();
		ctxMenuReboot = new ToolStripMenuItem();
		ctxMenuEdit = new ToolStripMenuItem();
		ctxMenuDelete = new ToolStripMenuItem();
		statusStrip = new StatusStrip();
		statusMessage = new ToolStripStatusLabel();
		statusSpring = new ToolStripStatusLabel();
		statusClock = new ToolStripStatusLabel();
		clockTimer = new System.Windows.Forms.Timer(components);
		toolStrip.SuspendLayout();
		((System.ComponentModel.ISupportInitialize)splitMain).BeginInit();
		splitMain.Panel1.SuspendLayout();
		splitMain.Panel2.SuspendLayout();
		splitMain.SuspendLayout();
		((System.ComponentModel.ISupportInitialize)grid).BeginInit();
		ctxMenuGrid.SuspendLayout();
		statusStrip.SuspendLayout();
		SuspendLayout();
		// 
		// toolStrip
		// 
		toolStrip.GripStyle = ToolStripGripStyle.Hidden;
		toolStrip.Items.AddRange(new ToolStripItem[] { btnAdd, btnEdit, btnDelete, sep1, btnUp, btnDown, sep2, btnReboot });
		toolStrip.Location = new Point(0, 0);
		toolStrip.Name = "toolStrip";
		toolStrip.Size = new Size(900, 25);
		toolStrip.TabIndex = 0;
		toolStrip.Text = "toolStrip";
		// 
		// btnAdd
		// 
		btnAdd.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnAdd.Name = "btnAdd";
		btnAdd.Size = new Size(45, 22);
		btnAdd.Text = "+ Add";
		// 
		// btnEdit
		// 
		btnEdit.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnEdit.Name = "btnEdit";
		btnEdit.Size = new Size(31, 22);
		btnEdit.Text = "Edit";
		// 
		// btnDelete
		// 
		btnDelete.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnDelete.Name = "btnDelete";
		btnDelete.Size = new Size(54, 22);
		btnDelete.Text = "- Delete";
		// 
		// sep1
		// 
		sep1.Name = "sep1";
		sep1.Size = new Size(6, 25);
		// 
		// btnUp
		// 
		btnUp.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnUp.Name = "btnUp";
		btnUp.Size = new Size(23, 22);
		btnUp.Text = "↑";
		// 
		// btnDown
		// 
		btnDown.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnDown.Name = "btnDown";
		btnDown.Size = new Size(23, 22);
		btnDown.Text = "↓";
		// 
		// sep2
		// 
		sep2.Name = "sep2";
		sep2.Size = new Size(6, 25);
		// 
		// btnReboot
		// 
		btnReboot.DisplayStyle = ToolStripItemDisplayStyle.Text;
		btnReboot.Name = "btnReboot";
		btnReboot.Size = new Size(49, 22);
		btnReboot.Text = "Reboot";
		// 
		// splitMain
		// 
		splitMain.Dock = DockStyle.Fill;
		splitMain.Location = new Point(0, 25);
		splitMain.Name = "splitMain";
		splitMain.Orientation = Orientation.Horizontal;
		// 
		// splitMain.Panel1
		// 
		splitMain.Panel1.Controls.Add(grid);
		// 
		// splitMain.Panel2
		// 
		splitMain.Panel2.Controls.Add(txtDetail);
		splitMain.Size = new Size(900, 607);
		splitMain.SplitterDistance = 436;
		splitMain.TabIndex = 1;
		// 
		// grid
		// 
		grid.AllowUserToAddRows = false;
		grid.AllowUserToDeleteRows = false;
		grid.AllowUserToResizeRows = false;
		grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
		grid.ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.AutoSize;
		grid.Columns.AddRange(new DataGridViewColumn[] { ColNo, ColStatus, ColDeviceName, ColDeviceId, ColPc, ColGpio, ColFw, ColUptime, ColLastSeen });
		grid.Dock = DockStyle.Fill;
		grid.Location = new Point(0, 0);
		grid.MultiSelect = false;
		grid.Name = "grid";
		grid.ReadOnly = true;
		grid.RowHeadersVisible = false;
		grid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
		grid.Size = new Size(900, 436);
		grid.TabIndex = 0;
		// 
		// ColNo
		// 
		dataGridViewCellStyle1.Alignment = DataGridViewContentAlignment.MiddleCenter;
		ColNo.DefaultCellStyle = dataGridViewCellStyle1;
		ColNo.FillWeight = 5F;
		ColNo.HeaderText = "No.";
		ColNo.Name = "ColNo";
		ColNo.ReadOnly = true;
		// 
		// ColStatus
		// 
		dataGridViewCellStyle2.Alignment = DataGridViewContentAlignment.MiddleCenter;
		ColStatus.DefaultCellStyle = dataGridViewCellStyle2;
		ColStatus.FillWeight = 9F;
		ColStatus.HeaderText = "연결";
		ColStatus.Name = "ColStatus";
		ColStatus.ReadOnly = true;
		// 
		// ColDeviceName
		// 
		ColDeviceName.FillWeight = 18F;
		ColDeviceName.HeaderText = "기기 이름";
		ColDeviceName.Name = "ColDeviceName";
		ColDeviceName.ReadOnly = true;
		// 
		// ColDeviceId
		// 
		ColDeviceId.FillWeight = 14F;
		ColDeviceId.HeaderText = "기기 ID";
		ColDeviceId.Name = "ColDeviceId";
		ColDeviceId.ReadOnly = true;
		// 
		// ColPc
		// 
		ColPc.FillWeight = 12F;
		ColPc.HeaderText = "PC";
		ColPc.Name = "ColPc";
		ColPc.ReadOnly = true;
		// 
		// ColGpio
		// 
		ColGpio.FillWeight = 12F;
		ColGpio.HeaderText = "GPIO";
		ColGpio.Name = "ColGpio";
		ColGpio.ReadOnly = true;
		// 
		// ColFw
		// 
		ColFw.FillWeight = 14F;
		ColFw.HeaderText = "FW";
		ColFw.Name = "ColFw";
		ColFw.ReadOnly = true;
		// 
		// ColUptime
		// 
		ColUptime.FillWeight = 16F;
		ColUptime.HeaderText = "Uptime";
		ColUptime.Name = "ColUptime";
		ColUptime.ReadOnly = true;
		// 
		// ColLastSeen
		// 
		ColLastSeen.FillWeight = 18F;
		ColLastSeen.HeaderText = "Last Seen";
		ColLastSeen.Name = "ColLastSeen";
		ColLastSeen.ReadOnly = true;
		// 
		// txtDetail
		// 
		txtDetail.Dock = DockStyle.Fill;
		txtDetail.Font = new Font("Consolas", 9F);
		txtDetail.Location = new Point(0, 0);
		txtDetail.Multiline = true;
		txtDetail.Name = "txtDetail";
		txtDetail.ReadOnly = true;
		txtDetail.ScrollBars = ScrollBars.Vertical;
		txtDetail.Size = new Size(900, 167);
		txtDetail.TabIndex = 0;
		txtDetail.WordWrap = false;
		// 
		// ctxMenuGrid
		// 
		ctxMenuGrid.Items.AddRange(new ToolStripItem[] { ctxMenuOpenBrowser, ctxMenuReboot, ctxMenuEdit, ctxMenuDelete });
		ctxMenuGrid.Name = "ctxMenuGrid";
		ctxMenuGrid.Size = new Size(160, 92);
		//
		// ctxMenuOpenBrowser
		//
		ctxMenuOpenBrowser.Name = "ctxMenuOpenBrowser";
		ctxMenuOpenBrowser.Size = new Size(159, 22);
		ctxMenuOpenBrowser.Text = "브라우저로 열기";
		//
		// ctxMenuReboot
		//
		ctxMenuReboot.Name = "ctxMenuReboot";
		ctxMenuReboot.Size = new Size(159, 22);
		ctxMenuReboot.Text = "Reboot";
		// 
		// ctxMenuEdit
		// 
		ctxMenuEdit.Name = "ctxMenuEdit";
		ctxMenuEdit.Size = new Size(112, 22);
		ctxMenuEdit.Text = "Edit...";
		// 
		// ctxMenuDelete
		// 
		ctxMenuDelete.Name = "ctxMenuDelete";
		ctxMenuDelete.Size = new Size(112, 22);
		ctxMenuDelete.Text = "Delete";
		// 
		// statusStrip
		// 
		statusStrip.Items.AddRange(new ToolStripItem[] { statusMessage, statusSpring, statusClock });
		statusStrip.Location = new Point(0, 632);
		statusStrip.Name = "statusStrip";
		statusStrip.Size = new Size(900, 22);
		statusStrip.TabIndex = 2;
		statusStrip.Text = "statusStrip";
		// 
		// statusMessage
		// 
		statusMessage.Name = "statusMessage";
		statusMessage.Size = new Size(39, 17);
		statusMessage.Text = "Ready";
		// 
		// statusSpring
		// 
		statusSpring.Name = "statusSpring";
		statusSpring.Size = new Size(803, 17);
		statusSpring.Spring = true;
		// 
		// statusClock
		// 
		statusClock.Name = "statusClock";
		statusClock.Size = new Size(43, 17);
		statusClock.Text = "--:--:--";
		// 
		// clockTimer
		// 
		clockTimer.Interval = 1000;
		// 
		// MainForm
		// 
		AutoScaleDimensions = new SizeF(7F, 15F);
		AutoScaleMode = AutoScaleMode.Font;
		ClientSize = new Size(900, 654);
		Controls.Add(splitMain);
		Controls.Add(statusStrip);
		Controls.Add(toolStrip);
		Name = "MainForm";
		StartPosition = FormStartPosition.CenterScreen;
		Text = "Smart IoT Hub 통합 관리툴";
		toolStrip.ResumeLayout(false);
		toolStrip.PerformLayout();
		splitMain.Panel1.ResumeLayout(false);
		splitMain.Panel2.ResumeLayout(false);
		splitMain.Panel2.PerformLayout();
		((System.ComponentModel.ISupportInitialize)splitMain).EndInit();
		splitMain.ResumeLayout(false);
		((System.ComponentModel.ISupportInitialize)grid).EndInit();
		ctxMenuGrid.ResumeLayout(false);
		statusStrip.ResumeLayout(false);
		statusStrip.PerformLayout();
		ResumeLayout(false);
		PerformLayout();
	}

	#endregion

	private System.Windows.Forms.ToolStrip toolStrip;
    private System.Windows.Forms.ToolStripButton btnAdd;
    private System.Windows.Forms.ToolStripButton btnEdit;
    private System.Windows.Forms.ToolStripButton btnDelete;
    private System.Windows.Forms.ToolStripSeparator sep1;
    private System.Windows.Forms.ToolStripButton btnUp;
    private System.Windows.Forms.ToolStripButton btnDown;
    private System.Windows.Forms.ToolStripSeparator sep2;
    private System.Windows.Forms.ToolStripButton btnReboot;
    private System.Windows.Forms.SplitContainer splitMain;
    private System.Windows.Forms.DataGridView grid;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColNo;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColStatus;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColDeviceName;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColDeviceId;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColPc;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColGpio;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColFw;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColUptime;
    private System.Windows.Forms.DataGridViewTextBoxColumn ColLastSeen;
    private System.Windows.Forms.TextBox txtDetail;
    private System.Windows.Forms.ContextMenuStrip ctxMenuGrid;
    private System.Windows.Forms.ToolStripMenuItem ctxMenuOpenBrowser;
    private System.Windows.Forms.ToolStripMenuItem ctxMenuReboot;
    private System.Windows.Forms.ToolStripMenuItem ctxMenuEdit;
    private System.Windows.Forms.ToolStripMenuItem ctxMenuDelete;
    private System.Windows.Forms.StatusStrip statusStrip;
    private System.Windows.Forms.ToolStripStatusLabel statusMessage;
    private System.Windows.Forms.ToolStripStatusLabel statusSpring;
    private System.Windows.Forms.ToolStripStatusLabel statusClock;
    private System.Windows.Forms.Timer clockTimer;
}
