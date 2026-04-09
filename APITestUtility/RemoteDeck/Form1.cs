using System.Xml.Serialization;
using System;
using System.Text;
using System.Windows.Forms;
using uPLibrary.Networking.M2Mqtt;
using uPLibrary.Networking.M2Mqtt.Messages;
using Newtonsoft.Json;
using System.Collections.Generic;
using System.IO.Ports;

namespace RemoteDeck
{
	public partial class Form1 : Form
	{
		public Form1()
		{
			InitializeComponent();
		}

		ConfigSet m_objConfig;
		DeckStatus m_Status;

		Button[] m_RelayOnBtns;
		Button[] m_RelayOffBtns;
		Button[] m_IOonBtns;
		Button[] m_IOoffBtns;

		private MqttClient client;
		private string clientId = Guid.NewGuid().ToString();


		private void Form1_Load(object sender, EventArgs e)
		{
		
			
			m_objConfig = ConfigSet.GetConfig();

			txtIp.Text = m_objConfig.IP;
			txtPort.Text = m_objConfig.Port;
			txtSubcribe.Text = m_objConfig.Subscribe;
			txtPublish.Text = m_objConfig.Publish;

			m_RelayOnBtns = new Button[] { btnRelay1ON, btnRelay2ON };
			m_RelayOffBtns = new Button[] { btnRelay1OFF, btnRelay2OFF };
			m_IOonBtns = new Button[] { btnIO1ON, btnIO2ON, btnIO3ON, btnIO4ON, btnIO5ON, btnIO6ON, btnIO7ON };
			m_IOoffBtns = new Button[] { btnIO1OFF, btnIO2OFF, btnIO3OFF, btnIO4OFF, btnIO5OFF, btnIO6OFF, btnIO7OFF };
		}

		private void Form1_FormClosing(object sender, FormClosingEventArgs e)
		{
			//Mqtt 서버 연결 종료
			try
			{
				client.Disconnect();
			}
			catch (Exception ex)
			{
			}
		}

		private void btnConfigSave_Click(object sender, EventArgs e)
		{
			m_objConfig.IP = txtIp.Text;
			m_objConfig.Port = txtPort.Text;
			m_objConfig.Subscribe = txtSubcribe.Text;
			m_objConfig.Publish = txtPublish.Text;
			m_objConfig.Save();

			MessageBox.Show("Mqtt 서버 정보가 저장되었습니다.");
		}

		private void Client_MqttMsgPublishReceived(object sender, MqttMsgPublishEventArgs e)
		{
			// 수신된 메시지를 UI 스레드에서 처리하기 위해 Invoke 사용
			this.Invoke((MethodInvoker)(() =>
			{
				// UI 컨트롤 접근은 Invoke로 처리
				this.Invoke((MethodInvoker)(() =>
				{
					string receivedMessage = Encoding.UTF8.GetString(e.Message);

					// 수신 메시지를 listBox에 먼저 찍어봄
					richLogger.AppendText($"[수신 RAW] {receivedMessage}\r\n");

					// JSON 파싱 시도
					ParseReceivedMessage(receivedMessage);
				}));
			}));
		}

		private bool mqttSubscribe()
		{
			if (client == null || !client.IsConnected)
			{
				MessageBox.Show("먼저 브로커에 연결하세요.");
				return false;
			}

			string topic = txtSubcribe.Text.Trim();
			if (string.IsNullOrEmpty(topic))
			{
				MessageBox.Show("구독 토픽을 입력하세요.");
				return false;
			}

			// 구독 (QoS 0, 1, 2 중 선택 가능)
			client.Subscribe(new string[] { topic }, new byte[] { MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE });
			//MessageBox.Show($"Topic '{topic}' 구독 완료.");
			richLogger.AppendText($"Topic '{topic}' 구독 완료.\r\n");

			return true;
		}

		private bool mqttPublish(CommandMessage cmdMessage)
		{
			if (client == null || !client.IsConnected)
			{
				MessageBox.Show("먼저 브로커에 연결하세요.");
				return false;
			}

			string topic = txtPublish.Text.Trim();
			if (string.IsNullOrEmpty(topic))
			{
				MessageBox.Show("전송 토픽을 입력하세요.");
				return false;
			}

			// JSON 직렬화
			string jsonString = JsonConvert.SerializeObject(cmdMessage);

			// 발행
			client.Publish(topic, Encoding.UTF8.GetBytes(jsonString), MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE, false);
			//MessageBox.Show("JSON 메시지 발행 완료.");
			richLogger.AppendText($"[COMMAND]→ Command:{cmdMessage.Command}, Seq:{cmdMessage.Sequence}, Data:{cmdMessage.Data}, Message:{cmdMessage.Message}\r\n");

			return true;
		}

		/// <summary>
		/// 수신된 메시지를 JSON 형태로 파싱하여 처리
		/// </summary>
		/// <param name="jsonMessage">수신된 메시지(문자열)</param>
		private void ParseReceivedMessage(string jsonMessage)
		{
			// 1) StatusMessage 형태로 먼저 시도
			try
			{
				var extStatusObj = JsonConvert.DeserializeObject<StatusMessage>(jsonMessage);

				// device_id, status, message 필드가 모두 존재할 가능성이 있음
				// status가 null이 아니라면 유효한 구조로 간주
				if (extStatusObj != null && extStatusObj.Status != null)
				{
					// device_id, message 등 확인
					richLogger.AppendText($"→ [DEVICE_ID] {extStatusObj.DeviceId}");
					richLogger.AppendText($"→ [MESSAGE] {extStatusObj.Message}");

					// status 배열 출력
					foreach (var st in extStatusObj.Status)
					{
						richLogger.AppendText(
							$"→ [STATUS] Type:{st.Type}, Seq:{st.Sequence}, Data:{st.Data}"
						);
					}
					return;
				}
			}
			catch
			{
				// 무시하고 다음으로 넘어감
			}

			// 3) 만약 어느 형태도 아니면 일반 문자열로 처리
			richLogger.AppendText("→ [STRING or Unknown JSON] " + jsonMessage + "\r\n");
		}

		private void btnMqttConn_Click(object sender, EventArgs e)
		{
			//Mqtt 서버에 연결
			try
			{
				string brokerAddress = txtIp.Text.Trim();
				int port = int.Parse(txtPort.Text.Trim());

				// MqttClient 초기화
				client = new MqttClient(brokerAddress, port, false, null, null, MqttSslProtocols.None);

				// 메시지 수신 콜백 지정
				client.MqttMsgPublishReceived += Client_MqttMsgPublishReceived;

				// Connect(사용자명/패스워드가 필요한 경우엔 해당 인자를 추가)
				client.Connect(clientId);

				if (client.IsConnected)
				{
					richLogger.AppendText("MQTT Broker에 연결되었습니다.\r\n");
				}
				else
				{
					richLogger.AppendText("MQTT Broker 연결 실패.\r\n");
				}

				mqttSubscribe();

				var cmdMessage = new CommandMessage
				{
					Command = "GETSTATUS",
					Sequence = 0,
					Data = 0,
					Message = ""
				};

				mqttPublish(cmdMessage);
			}
			catch (Exception ex)
			{
				MessageBox.Show("오류: " + ex.Message);
			}
		}

		private void btnMqttDiscon_Click(object sender, EventArgs e)
		{
			//Mqtt 서버 연결 종료
			try
			{
				client.Disconnect();

				richLogger.AppendText("MQTT Broker에 연결이 종료되었습니다.\r\n");
			}
			catch (Exception ex)
			{
				MessageBox.Show("오류: " + ex.Message);
			}
		}

		private void btnClearLog_Click(object sender, EventArgs e)
		{
			richLogger.Clear();
		}

		private void btnGetAllStatus_Click(object sender, EventArgs e)
		{
			//전체 상태 요청
			var cmdMessage = new CommandMessage
			{
				Command = CommandType.PCLED.ToString(),
				Sequence = 1,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnRelayON_Click(object sender, EventArgs e)
		{
			//Relay 1,2 On 명령 전송

			string strNo = ((Button)sender).Name;
			int nNo = int.Parse(strNo.Substring(8, 1));

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.RELAY.ToString(),
				Sequence = nNo,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnRelayOFF_Click(object sender, EventArgs e)
		{
			//Relay 1, 2 Off 명령 전송

			string strNo = ((Button)sender).Name;
			int nNo = int.Parse(strNo.Substring(8, 1));

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.RELAY.ToString(),
				Sequence = nNo,
				Data = 0,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnIOON_Click(object sender, EventArgs e)
		{
			//IO 1~7 On 명령 전송

			string strNo = ((Button)sender).Name;
			int nNo = int.Parse(strNo.Substring(5, 1));

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.GPIO.ToString(),
				Sequence = nNo,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);

		}

		private void btnIOOFF_Click(object sender, EventArgs e)
		{
			//IO 1~7 Off 명령 전송

			string strNo = ((Button)sender).Name;
			int nNo = int.Parse(strNo.Substring(5, 1));

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.GPIO.ToString(),
				Sequence = nNo,
				Data = 0,
				Message = "null"
			};

			mqttPublish(cmdMessage);

		}

		private void btnGetLed_Click(object sender, EventArgs e)
		{
			//PC Led 상태 요청 전송

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.PCLED.ToString(),
				Sequence = 1,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnIrON_Click(object sender, EventArgs e)
		{
			//IR ON 명령 전송
			var cmdMessage = new CommandMessage
			{
				Command = CommandType.IR.ToString(),
				Sequence = 1,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnIrOFF_Click(object sender, EventArgs e)
		{
			//IR OFF 명령 전송
			var cmdMessage = new CommandMessage
			{
				Command = CommandType.IR.ToString(),
				Sequence = 1,
				Data = 0,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnHdmiON_Click(object sender, EventArgs e)
		{
			//HDMI ON 명령 전송

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.HDMI.ToString(),
				Sequence = 1,
				Data = 1,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btnHdmiOFF_Click(object sender, EventArgs e)
		{
			//HDMI OFF 명령 전송

			var cmdMessage = new CommandMessage
			{
				Command = CommandType.HDMI.ToString(),
				Sequence = 1,
				Data = 0,
				Message = "null"
			};

			mqttPublish(cmdMessage);
		}

		private void btn485Conn_Click(object sender, EventArgs e)
		{
			//시리얼 포트 통신 연결
		}

		private void btn485Disconn_Click(object sender, EventArgs e)
		{
			//시리얼 포트 통신 연결 종료
		}

		private void btn485Relay1ON_Click(object sender, EventArgs e)
		{
			//Relay1 ON 명령을 시리얼 포트로 전송
		}

		private void btn485Relay1OFF_Click(object sender, EventArgs e)
		{
			//Relay1 OFF 명령을 시리얼 포트로 전송
		}	
	}

	struct DeckStatus
	{
		bool Relay1;
		bool Relay2;
		bool Io1;
		bool Io2;
		bool Io3;
		bool Io4;
		bool Io5;
		bool Io6;
		bool Io7;
		bool PCLamp;
	}

	[Serializable]
	public class ConfigSet
	{
		public string IP;
		public string Port;
		public string Publish;
		public string Subscribe;

		public void Save()
		{
			string filename = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath) + "\\", "ConfigSet.xml");
			XmlSerializer ser = new XmlSerializer(typeof(ConfigSet));
			using (TextWriter writer = new StreamWriter(filename))
			{
				ser.Serialize(writer, this);
			}
		}

		public static ConfigSet GetConfig()
		{
			ConfigSet En;

			string filename = Path.Combine(Path.GetDirectoryName(Application.ExecutablePath) + "\\", "ConfigSet.xml");

			if (!System.IO.File.Exists(filename))
			{
				En = new ConfigSet();
				En.IP = "127.0.0.1";
				En.Port = "1883";
				En.Subscribe = "RemoteDeck/Server";
				En.Publish = "RemoteDeck/node_1";

				return En;
			}

			XmlSerializer ser = new XmlSerializer(typeof(ConfigSet));
			using (TextReader reader = new StreamReader(filename))
			{
				En = (ConfigSet)ser.Deserialize(reader);
			}

			return En;
		}
	}

	/// <summary>
	/// 발행(요청) 메시지 구조 (예: command: "RELAY" 등)
	/// </summary>
	public class CommandMessage
	{
		[JsonProperty("command")]
		public string Command { get; set; }

		[JsonProperty("sequence")]
		public int Sequence { get; set; }

		[JsonProperty("data")]
		public int Data { get; set; }

		[JsonProperty("message")]
		public string Message { get; set; }
	}

	/// <summary>
	/// (수신) 확장된 StatusMessage: device_id, status[], message 포함
	/// </summary>
	public class StatusMessage
	{
		[Newtonsoft.Json.JsonProperty("device_id")]
		public string DeviceId { get; set; }

		[Newtonsoft.Json.JsonProperty("status")]
		public List<StatusItem> Status { get; set; }

		[Newtonsoft.Json.JsonProperty("message")]
		public string Message { get; set; }
	}

	/// <summary>
	/// status 배열 안의 개별 항목 구조
	/// </summary>
	public class StatusItem
	{
		[Newtonsoft.Json.JsonProperty("type")]
		public string Type { get; set; }

		[Newtonsoft.Json.JsonProperty("sequence")]
		public int Sequence { get; set; }

		[Newtonsoft.Json.JsonProperty("data")]
		public int Data { get; set; }
	}

	public enum CommandType
	{
		RELAY,
		GPIO,
		PCLED,
		IR,
		HDMI,		
		GETSTATUS,
	}

	/*		파싱 및 송수신 데이타 참조

			1. PC -> Device 전송데이티(명령) 정의

			타잎 : json 문자열
			형식 :

			{
				"command" : "RELAY",	//(명령 : RELAY, GPIO, PCLED, IR, HDMI, GETLED, GETSTATUS, ...) 	
				"sequence" : 1,			//(단자번호 :	0~7)
				"data" : 1				//(상태값 : 0 (off), 1 (on) )
			}

			2. Device -> PC 전송 데이타(상태) 정의

			{				
				"status": [
					{
						"type": "RELAY",	(단자종류 : RELAY, GPIO, LED, HDMI, ...)
						"sequence" : 1,		(단자번호 :	0~7)
						"data" : 1			(상태값 : 0 (off), 1 (on) )
					},
					{
						"type": "RELAY",
						"sequence" : 2,
						"data" : 0
					}, 
					{
						"type": "GPIO",
						"sequence" : 1,
						"data" : 1
					}     
				]
			}
		*/
}
