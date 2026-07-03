#nullable disable

namespace IntegrateController.UI;

partial class DeviceEditDialog
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
        this.components = new System.ComponentModel.Container();
        this.lblIp = new System.Windows.Forms.Label();
        this.txtIp = new System.Windows.Forms.TextBox();
        this.lblPort = new System.Windows.Forms.Label();
        this.numPort = new System.Windows.Forms.NumericUpDown();
        this.lblUser = new System.Windows.Forms.Label();
        this.txtUser = new System.Windows.Forms.TextBox();
        this.lblPassword = new System.Windows.Forms.Label();
        this.txtPassword = new System.Windows.Forms.TextBox();
        this.lblInterval = new System.Windows.Forms.Label();
        this.numInterval = new System.Windows.Forms.NumericUpDown();
        this.lblTimeout = new System.Windows.Forms.Label();
        this.numTimeout = new System.Windows.Forms.NumericUpDown();
        this.btnOk = new System.Windows.Forms.Button();
        this.btnCancel = new System.Windows.Forms.Button();
        this.errorProvider = new System.Windows.Forms.ErrorProvider(this.components);
        ((System.ComponentModel.ISupportInitialize)(this.numPort)).BeginInit();
        ((System.ComponentModel.ISupportInitialize)(this.numInterval)).BeginInit();
        ((System.ComponentModel.ISupportInitialize)(this.numTimeout)).BeginInit();
        ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).BeginInit();
        this.SuspendLayout();
        //
        // lblIp
        //
        this.lblIp.Location = new System.Drawing.Point(16, 20);
        this.lblIp.Name = "lblIp";
        this.lblIp.Size = new System.Drawing.Size(110, 26);
        this.lblIp.TabIndex = 0;
        this.lblIp.Text = "IP";
        //
        // txtIp
        //
        this.txtIp.Location = new System.Drawing.Point(140, 16);
        this.txtIp.Name = "txtIp";
        this.txtIp.Size = new System.Drawing.Size(240, 23);
        this.txtIp.TabIndex = 1;
        //
        // lblPort
        //
        this.lblPort.Location = new System.Drawing.Point(16, 54);
        this.lblPort.Name = "lblPort";
        this.lblPort.Size = new System.Drawing.Size(110, 26);
        this.lblPort.TabIndex = 2;
        this.lblPort.Text = "Port";
        //
        // numPort
        //
        this.numPort.Location = new System.Drawing.Point(140, 50);
        this.numPort.Maximum = new decimal(new int[] { 65535, 0, 0, 0 });
        this.numPort.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
        this.numPort.Name = "numPort";
        this.numPort.Size = new System.Drawing.Size(240, 23);
        this.numPort.TabIndex = 3;
        this.numPort.Value = new decimal(new int[] { 5050, 0, 0, 0 });
        //
        // lblUser
        //
        this.lblUser.Location = new System.Drawing.Point(16, 88);
        this.lblUser.Name = "lblUser";
        this.lblUser.Size = new System.Drawing.Size(110, 26);
        this.lblUser.TabIndex = 4;
        this.lblUser.Text = "Auth User";
        //
        // txtUser
        //
        this.txtUser.Location = new System.Drawing.Point(140, 84);
        this.txtUser.Name = "txtUser";
        this.txtUser.Size = new System.Drawing.Size(240, 23);
        this.txtUser.TabIndex = 5;
        //
        // lblPassword
        //
        this.lblPassword.Location = new System.Drawing.Point(16, 122);
        this.lblPassword.Name = "lblPassword";
        this.lblPassword.Size = new System.Drawing.Size(110, 26);
        this.lblPassword.TabIndex = 6;
        this.lblPassword.Text = "Auth Pass";
        //
        // txtPassword
        //
        this.txtPassword.Location = new System.Drawing.Point(140, 118);
        this.txtPassword.Name = "txtPassword";
        this.txtPassword.Size = new System.Drawing.Size(240, 23);
        this.txtPassword.TabIndex = 7;
        this.txtPassword.UseSystemPasswordChar = true;
        //
        // lblInterval
        //
        this.lblInterval.Location = new System.Drawing.Point(16, 156);
        this.lblInterval.Name = "lblInterval";
        this.lblInterval.Size = new System.Drawing.Size(110, 26);
        this.lblInterval.TabIndex = 8;
        this.lblInterval.Text = "Poll Interval (s)";
        //
        // numInterval
        //
        this.numInterval.Location = new System.Drawing.Point(140, 152);
        this.numInterval.Maximum = new decimal(new int[] { 30, 0, 0, 0 });
        this.numInterval.Minimum = new decimal(new int[] { 1, 0, 0, 0 });
        this.numInterval.Name = "numInterval";
        this.numInterval.Size = new System.Drawing.Size(240, 23);
        this.numInterval.TabIndex = 9;
        this.numInterval.Value = new decimal(new int[] { 3, 0, 0, 0 });
        //
        // lblTimeout
        //
        this.lblTimeout.Location = new System.Drawing.Point(16, 190);
        this.lblTimeout.Name = "lblTimeout";
        this.lblTimeout.Size = new System.Drawing.Size(110, 26);
        this.lblTimeout.TabIndex = 10;
        this.lblTimeout.Text = "Timeout (ms)";
        //
        // numTimeout
        //
        this.numTimeout.Increment = new decimal(new int[] { 100, 0, 0, 0 });
        this.numTimeout.Location = new System.Drawing.Point(140, 186);
        this.numTimeout.Maximum = new decimal(new int[] { 10000, 0, 0, 0 });
        this.numTimeout.Minimum = new decimal(new int[] { 500, 0, 0, 0 });
        this.numTimeout.Name = "numTimeout";
        this.numTimeout.Size = new System.Drawing.Size(240, 23);
        this.numTimeout.TabIndex = 11;
        this.numTimeout.Value = new decimal(new int[] { 2000, 0, 0, 0 });
        //
        // btnOk
        //
        this.btnOk.Location = new System.Drawing.Point(184, 228);
        this.btnOk.Name = "btnOk";
        this.btnOk.Size = new System.Drawing.Size(96, 30);
        this.btnOk.TabIndex = 12;
        this.btnOk.Text = "OK";
        this.btnOk.UseVisualStyleBackColor = true;
        //
        // btnCancel
        //
        this.btnCancel.Location = new System.Drawing.Point(284, 228);
        this.btnCancel.Name = "btnCancel";
        this.btnCancel.Size = new System.Drawing.Size(96, 30);
        this.btnCancel.TabIndex = 13;
        this.btnCancel.Text = "Cancel";
        this.btnCancel.UseVisualStyleBackColor = true;
        //
        // errorProvider
        //
        this.errorProvider.BlinkStyle = System.Windows.Forms.ErrorBlinkStyle.NeverBlink;
        this.errorProvider.ContainerControl = this;
        //
        // DeviceEditDialog
        //
        this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
        this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
        this.ClientSize = new System.Drawing.Size(400, 276);
        this.Controls.Add(this.lblIp);
        this.Controls.Add(this.txtIp);
        this.Controls.Add(this.lblPort);
        this.Controls.Add(this.numPort);
        this.Controls.Add(this.lblUser);
        this.Controls.Add(this.txtUser);
        this.Controls.Add(this.lblPassword);
        this.Controls.Add(this.txtPassword);
        this.Controls.Add(this.lblInterval);
        this.Controls.Add(this.numInterval);
        this.Controls.Add(this.lblTimeout);
        this.Controls.Add(this.numTimeout);
        this.Controls.Add(this.btnOk);
        this.Controls.Add(this.btnCancel);
        this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
        this.MaximizeBox = false;
        this.MinimizeBox = false;
        this.Name = "DeviceEditDialog";
        this.ShowInTaskbar = false;
        this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
        this.Text = "Device";
        ((System.ComponentModel.ISupportInitialize)(this.numPort)).EndInit();
        ((System.ComponentModel.ISupportInitialize)(this.numInterval)).EndInit();
        ((System.ComponentModel.ISupportInitialize)(this.numTimeout)).EndInit();
        ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).EndInit();
        this.ResumeLayout(false);
        this.PerformLayout();
    }

    #endregion

    private System.Windows.Forms.Label lblIp;
    private System.Windows.Forms.TextBox txtIp;
    private System.Windows.Forms.Label lblPort;
    private System.Windows.Forms.NumericUpDown numPort;
    private System.Windows.Forms.Label lblUser;
    private System.Windows.Forms.TextBox txtUser;
    private System.Windows.Forms.Label lblPassword;
    private System.Windows.Forms.TextBox txtPassword;
    private System.Windows.Forms.Label lblInterval;
    private System.Windows.Forms.NumericUpDown numInterval;
    private System.Windows.Forms.Label lblTimeout;
    private System.Windows.Forms.NumericUpDown numTimeout;
    private System.Windows.Forms.Button btnOk;
    private System.Windows.Forms.Button btnCancel;
    private System.Windows.Forms.ErrorProvider errorProvider;
}
