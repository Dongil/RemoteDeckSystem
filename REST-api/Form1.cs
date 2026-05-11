using System;
using System.Collections.Generic;
using System.Drawing;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace REST_api
{
	public partial class Form1 : Form
	{
		// ────── Fields ──────
		// HttpClient properties (BaseAddress, DefaultRequestHeaders) are immutable after first
		// request, so we recreate the instance on every connect to support reconnection.
		private HttpClient _http;
		private readonly Timer _pollTimer = new Timer { Interval = 1000 };
		private readonly JavaScriptSerializer _json = new JavaScriptSerializer();
		private bool _isConnected = false;
		private int _failureCount = 0;
		private const int MAX_LOG_LINES = 1000;
		private const int MAX_FAILURES = 3;

		public Form1()
		{
			InitializeComponent();
			WireUp();
		}

		// ────── Init ──────
		private void WireUp()
		{
			this.Text = "RemoteDeck HTTP Sample";
			this.Load += Form1_Load;
			btnConn.Click += btnConn_Click;
			btnRelay1On.Click += (s, e) => { var _ = SendRelayAsync(1, "on"); };
			btnRelay1Off.Click += (s, e) => { var _ = SendRelayAsync(1, "off"); };
			btnRelay1Pluse.Click += (s, e) => { var _ = SendPulseAsync(1); };
			btnRelay2On.Click += (s, e) => { var _ = SendRelayAsync(2, "on"); };
			btnRelay2Off.Click += (s, e) => { var _ = SendRelayAsync(2, "off"); };
			btnRelay2Pluse.Click += (s, e) => { var _ = SendPulseAsync(2); };
			_pollTimer.Tick += async (s, e) =>
			{
				var st = await FetchStatusAsync();
				if (st != null) UpdateRelayUI(st.relay1 ? 1 : 0, st.relay2 ? 1 : 0);
			};
			textPW.PasswordChar = '*';
			SetControlsEnabled(false);
		}

		private void Form1_Load(object sender, EventArgs e)
		{
			if (string.IsNullOrEmpty(textIP.Text)) textIP.Text = "192.168.1.200";
			if (string.IsNullOrEmpty(textPort.Text)) textPort.Text = "5050";
			if (string.IsNullOrEmpty(textID.Text)) textID.Text = "admin";
			if (string.IsNullOrEmpty(textPW.Text)) textPW.Text = "12345";
		}

		// ────── Start/Stop toggle ──────
		private async void btnConn_Click(object sender, EventArgs e)
		{
			if (_isConnected) { DisconnectAndReset(); return; }

			btnConn.Enabled = false;
			try
			{
				var ip = textIP.Text.Trim();
				var port = textPort.Text.Trim();
				var id = textID.Text;
				var pw = textPW.Text;

				if (_http != null) { _http.Dispose(); _http = null; }
				_http = new HttpClient
				{
					Timeout = TimeSpan.FromSeconds(3),
					BaseAddress = new Uri("http://" + ip + ":" + port + "/")
				};
				var basic = Convert.ToBase64String(Encoding.ASCII.GetBytes(id + ":" + pw));
				_http.DefaultRequestHeaders.Authorization =
					new AuthenticationHeaderValue("Basic", basic);

				Log("CONNECT -> http://" + ip + ":" + port + "/  (user=" + id + ")");
				var st = await FetchStatusAsync(true);
				if (st != null)
				{
					_isConnected = true;
					_failureCount = 0;
					btnConn.Text = "Stop";
					SetControlsEnabled(true);
					UpdateRelayUI(st.relay1 ? 1 : 0, st.relay2 ? 1 : 0);
					_pollTimer.Start();
					Log("CONNECTED, polling 1s");
				}
			}
			catch (Exception ex) { Log("CONNECT ERROR: " + ex.Message); }
			finally { btnConn.Enabled = true; }
		}

		private void DisconnectAndReset()
		{
			_pollTimer.Stop();
			_isConnected = false;
			_failureCount = 0;
			SetControlsEnabled(false);
			btnConn.Text = "Start";
			UpdateRelayUI(0, 0);
			Log("DISCONNECTED");
		}

		// ────── HTTP ──────
		// Returns parsed status, or null on failure. Caller decides when to apply UI.
		private async Task<StatusResponse> FetchStatusAsync(bool logRequest = false)
		{
			if (_http == null) return null;
			try
			{
				if (logRequest) Log("GET /api/status");
				using (var resp = await _http.GetAsync("api/status"))
				{
					var body = await resp.Content.ReadAsStringAsync();
					if (!resp.IsSuccessStatusCode)
					{
						Log("  <- " + (int)resp.StatusCode + " " + body);
						OnFailure();
						return null;
					}
					if (logRequest) Log("  <- 200 " + body);
					var s = _json.Deserialize<StatusResponse>(body);
					_failureCount = 0;
					return s;
				}
			}
			catch (Exception ex)
			{
				Log("GET /api/status ERROR: " + ex.Message);
				OnFailure();
				return null;
			}
		}

		private async Task SendRelayAsync(int relay, string state)
		{
			var payload = new Dictionary<string, object> {
				{ "relay", relay }, { "state", state }
			};
			await PostRelayAsync(payload, "relay" + relay + "=" + state);
		}

		private async Task SendPulseAsync(int relay)
		{
			var payload = new Dictionary<string, object> {
				{ "cmd", "pulse" }, { "relay", relay }
			};
			await PostRelayAsync(payload, "relay" + relay + "=pulse");
		}

		private async Task PostRelayAsync(Dictionary<string, object> payload, string tag)
		{
			if (_http == null) return;
			try
			{
				var body = _json.Serialize(payload);
				Log("POST /api/relay  " + body);
				using (var content = new StringContent(body, Encoding.UTF8, "application/json"))
				using (var resp = await _http.PostAsync("api/relay", content))
				{
					var rsp = await resp.Content.ReadAsStringAsync();
					Log("  <- " + (int)resp.StatusCode + " " + rsp);
				}
			}
			catch (Exception ex) { Log("POST " + tag + " ERROR: " + ex.Message); }
		}

		private void OnFailure()
		{
			_failureCount++;
			if (_failureCount >= MAX_FAILURES && _isConnected)
			{
				Log("Polling failed " + MAX_FAILURES + " times - auto disconnect");
				DisconnectAndReset();
			}
		}

		// ────── UI helpers ──────
		private void UpdateRelayUI(int r1, int r2)
		{
			// UseVisualStyleBackColor=true overrides BackColor on themed Windows,
			// so disable it when applying Red and restore for default Control color.
			btnRelay1On.UseVisualStyleBackColor = r1 != 1;
			btnRelay1On.BackColor = r1 == 1 ? Color.Red : SystemColors.Control;
			btnRelay2On.UseVisualStyleBackColor = r2 != 1;
			btnRelay2On.BackColor = r2 == 1 ? Color.Red : SystemColors.Control;
		}

		private void SetControlsEnabled(bool enabled)
		{
			btnRelay1On.Enabled = btnRelay1Off.Enabled = btnRelay1Pluse.Enabled = enabled;
			btnRelay2On.Enabled = btnRelay2Off.Enabled = btnRelay2Pluse.Enabled = enabled;
		}

		private void Log(string msg)
		{
			string line = DateTime.Now.ToString("HH:mm:ss.fff") + "  " + msg + Environment.NewLine;
			if (textLog.Lines.Length > MAX_LOG_LINES)
			{
				int keepCount = MAX_LOG_LINES / 2;
				var keep = new string[keepCount];
				Array.Copy(textLog.Lines, textLog.Lines.Length - keepCount, keep, 0, keepCount);
				textLog.Lines = keep;
			}
			textLog.AppendText(line);
			textLog.SelectionStart = textLog.Text.Length;
			textLog.ScrollToCaret();
		}

		// ────── DTO ──────
		// Firmware /api/status returns relay1/relay2 as JSON boolean (not int)
		private class StatusResponse
		{
			public bool relay1 { get; set; }
			public bool relay2 { get; set; }
		}
	}
}
