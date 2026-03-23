// esp32_controller_fixed.ino
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// 配置区
const char* ssid = "Mi 10S";
const char* password = "Aa1093464588";
const char* hostname = "esp32-controller";

WebServer server(80);
WebSocketsServer webSocket(81);
bool ws_connected = false;

// 上次命令的时间戳，避免重复处理
unsigned long lastCommandTime = 0;
String lastResponse = "";

// 发送命令到Windows
void sendToWindows(String command) {
  Serial.println(command);
  delay(10); // 确保数据发送完成
}

// 生成JSON命令
String buildCommand(String type, String data, String params = "") {
  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["data"] = data;
  
  if (params != "") {
    StaticJsonDocument<128> paramDoc;
    deserializeJson(paramDoc, params);
    doc["params"] = paramDoc.as<JsonObject>();
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

// WebSocket事件处理
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      ws_connected = true;
      webSocket.sendTXT(num, "{\"status\":\"connected\"}");
      break;
      
    case WStype_TEXT:
      {
        // 解析客户端命令
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
          String cmdType = doc["type"];
          String cmdData = doc["data"];
          String cmdParams = "";
          
          if (doc.containsKey("params")) {
            serializeJson(doc["params"], cmdParams);
          }
          
          // 构建命令
          String command = buildCommand(cmdType, cmdData, cmdParams);
          
          // 发送给Windows
          sendToWindows(command);
          
          // 记录命令时间
          lastCommandTime = millis();
          
          // 立即返回发送成功
          webSocket.sendTXT(num, "{\"status\":\"sent\",\"command\":\"" + cmdType + "\"}");
        } else {
          webSocket.sendTXT(num, "{\"status\":\"error\",\"message\":\"JSON解析失败\"}");
        }
      }
      break;
      
    case WStype_DISCONNECTED:
      ws_connected = false;
      break;
  }
}

// Web服务器路由
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Windows远程控制器</title>
    <style>
        /* 样式代码与之前相同，保持不变 */
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: rgba(255, 255, 255, 0.95);
            border-radius: 15px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            overflow: hidden;
        }
        header {
            background: linear-gradient(90deg, #4f46e5, #7c3aed);
            color: white;
            padding: 30px;
            text-align: center;
        }
        h1 {
            font-size: 2.5em;
            margin-bottom: 10px;
        }
        .status {
            display: inline-block;
            padding: 5px 15px;
            background: #10b981;
            border-radius: 20px;
            font-size: 0.9em;
            margin-top: 10px;
        }
        .status.disconnected { background: #ef4444; }
        .status.sent { background: #3b82f6; }
        .status.error { background: #ef4444; }
        
        .tabs {
            display: flex;
            background: #f1f5f9;
            border-bottom: 2px solid #e2e8f0;
        }
        .tab {
            flex: 1;
            padding: 20px;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s;
            font-weight: 600;
            color: #64748b;
        }
        .tab:hover {
            background: #e2e8f0;
        }
        .tab.active {
            background: white;
            color: #4f46e5;
            border-bottom: 3px solid #4f46e5;
        }
        
        .tab-content {
            padding: 30px;
            display: none;
        }
        .tab-content.active {
            display: block;
        }
        
        .command-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .command-card {
            background: white;
            border: 2px solid #e2e8f0;
            border-radius: 10px;
            padding: 20px;
            transition: all 0.3s;
            cursor: pointer;
        }
        .command-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.1);
            border-color: #4f46e5;
        }
        
        .command-card h3 {
            color: #334155;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .command-card h3 i {
            font-size: 1.2em;
        }
        
        .btn {
            background: #4f46e5;
            color: white;
            border: none;
            padding: 12px 25px;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            transition: all 0.3s;
            width: 100%;
            margin-top: 10px;
        }
        .btn:hover {
            background: #4338ca;
            transform: scale(1.05);
        }
        
        .input-group {
            margin-bottom: 20px;
        }
        .input-group label {
            display: block;
            margin-bottom: 8px;
            color: #475569;
            font-weight: 600;
        }
        .input-group input,
        .input-group select,
        .input-group textarea {
            width: 100%;
            padding: 12px;
            border: 2px solid #e2e8f0;
            border-radius: 8px;
            font-size: 1em;
            transition: border-color 0.3s;
        }
        .input-group input:focus,
        .input-group select:focus,
        .input-group textarea:focus {
            outline: none;
            border-color: #4f46e5;
        }
        
        .keyboard-shortcuts {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
            gap: 10px;
            margin-top: 15px;
        }
        .key-btn {
            background: #f1f5f9;
            border: 2px solid #cbd5e1;
            border-radius: 6px;
            padding: 10px;
            text-align: center;
            cursor: pointer;
            transition: all 0.2s;
        }
        .key-btn:hover {
            background: #4f46e5;
            color: white;
            border-color: #4f46e5;
        }
        
        .log-container {
            background: #1e293b;
            color: #e2e8f0;
            padding: 20px;
            border-radius: 8px;
            margin-top: 30px;
            max-height: 200px;
            overflow-y: auto;
            font-family: 'Consolas', monospace;
            font-size: 0.9em;
        }
        
        @media (max-width: 768px) {
            .command-grid {
                grid-template-columns: 1fr;
            }
            .tabs {
                flex-direction: column;
            }
        }
    </style>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
</head>
<body>
    <div class="container">
        <header>
            <h1><i class="fas fa-desktop"></i> ESP32 Windows远程控制器</h1>
            <p>通过Web界面静默控制Windows系统</p>
            <div class="status" id="status">正在连接...</div>
        </header>
        
        <div class="tabs">
            <div class="tab active" onclick="switchTab('keyboard')">
                <i class="fas fa-keyboard"></i> 键盘模拟
            </div>
            <div class="tab" onclick="switchTab('cmd')">
                <i class="fas fa-terminal"></i> 命令行
            </div>
            <div class="tab" onclick="switchTab('apps')">
                <i class="fas fa-rocket"></i> 应用程序
            </div>
            <div class="tab" onclick="switchTab('files')">
                <i class="fas fa-folder-open"></i> 文件管理
            </div>
            <div class="tab" onclick="switchTab('system')">
                <i class="fas fa-cog"></i> 系统控制
            </div>
        </div>
        
        <!-- 键盘模拟标签页 -->
        <div class="tab-content active" id="keyboard-tab">
            <div class="input-group">
                <label><i class="fas fa-font"></i> 输入文本</label>
                <input type="text" id="textInput" placeholder="输入要模拟输入的文本">
                <button class="btn" onclick="typeText()">模拟输入</button>
            </div>
            
            <h3><i class="fas fa-bolt"></i> 常用快捷键</h3>
            <div class="keyboard-shortcuts">
                <div class="key-btn" onclick="sendHotkey('ctrl+alt+delete')">Ctrl+Alt+Del</div>
                <div class="key-btn" onclick="sendHotkey('win+d')">Win+D</div>
                <div class="key-btn" onclick="sendHotkey('alt+tab')">Alt+Tab</div>
                <div class="key-btn" onclick="sendHotkey('ctrl+c')">Ctrl+C</div>
                <div class="key-btn" onclick="sendHotkey('ctrl+v')">Ctrl+V</div>
                <div class="key-btn" onclick="sendHotkey('alt+f4')">Alt+F4</div>
                <div class="key-btn" onclick="sendHotkey('win+r')">Win+R</div>
                <div class="key-btn" onclick="sendHotkey('win+e')">Win+E</div>
            </div>
        </div>
        
        <!-- 命令行标签页 -->
        <div class="tab-content" id="cmd-tab">
            <div class="input-group">
                <label><i class="fas fa-code"></i> 执行命令</label>
                <textarea id="commandInput" rows="3" placeholder="输入要执行的命令，例如: dir C:\\"></textarea>
                <button class="btn" onclick="executeCommand()"><i class="fas fa-play"></i> 执行命令</button>
            </div>
            
            <h3><i class="fas fa-history"></i> 常用命令</h3>
            <div class="command-grid">
                <div class="command-card" onclick="quickCmd('ipconfig')">
                    <h3><i class="fas fa-network-wired"></i> 网络信息</h3>
                    <p>显示IP配置信息</p>
                </div>
                <div class="command-card" onclick="quickCmd('tasklist')">
                    <h3><i class="fas fa-tasks"></i> 进程列表</h3>
                    <p>显示运行中的进程</p>
                </div>
                <div class="command-card" onclick="quickCmd('echo Hello World')">
                    <h3><i class="fas fa-terminal"></i> 测试命令</h3>
                    <p>测试命令执行</p>
                </div>
            </div>
        </div>
        
        <!-- 应用程序标签页 -->
        <div class="tab-content" id="apps-tab">
            <h3><i class="fas fa-window-restore"></i> 系统应用</h3>
            <div class="command-grid">
                <div class="command-card" onclick="openApp('notepad')">
                    <h3><i class="fas fa-sticky-note"></i> 记事本</h3>
                </div>
                <div class="command-card" onclick="openApp('calc')">
                    <h3><i class="fas fa-calculator"></i> 计算器</h3>
                </div>
                <div class="command-card" onclick="openApp('explorer')">
                    <h3><i class="fas fa-folder"></i> 文件资源管理器</h3>
                </div>
                <div class="command-card" onclick="openApp('cmd')">
                    <h3><i class="fas fa-terminal"></i> 命令提示符</h3>
                </div>
            </div>
        </div>
        
        <!-- 文件管理标签页 -->
        <div class="tab-content" id="files-tab">
            <div class="input-group">
                <label><i class="fas fa-folder-open"></i> 打开文件/文件夹</label>
                <input type="text" id="filePath" placeholder="输入完整路径，如 C:\\Windows">
                <button class="btn" onclick="openPath()">打开</button>
            </div>
        </div>
        
        <!-- 系统控制标签页 -->
        <div class="tab-content" id="system-tab">
            <h3><i class="fas fa-cog"></i> 系统操作</h3>
            <div class="command-grid">
                <div class="command-card" onclick="shutdown()">
                    <h3><i class="fas fa-power-off"></i> 关机</h3>
                </div>
                <div class="command-card" onclick="restart()">
                    <h3><i class="fas fa-redo"></i> 重启</h3>
                </div>
                <div class="command-card" onclick="lock()">
                    <h3><i class="fas fa-lock"></i> 锁定</h3>
                </div>
            </div>
        </div>
        
        <div class="log-container" id="log">
            <div>系统日志：</div>
        </div>
    </div>
    
    <script>
        let ws = null;
        let currentTab = 'keyboard';
        
        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.hostname}:81/`;
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                updateStatus('connected', '已连接');
                addLog('WebSocket连接成功');
            };
            
            ws.onclose = function() {
                updateStatus('disconnected', '连接断开');
                addLog('WebSocket连接断开，3秒后重连...');
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                updateStatus('error', '连接错误');
                addLog('WebSocket连接错误: ' + error);
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    addLog('服务器响应: ' + JSON.stringify(data));
                    
                    if (data.status === 'connected') {
                        updateStatus('connected', '已连接');
                    } else if (data.status === 'sent') {
                        updateStatus('sent', '命令已发送: ' + data.command);
                        setTimeout(() => updateStatus('connected', '已连接'), 2000);
                    } else if (data.status === 'error') {
                        updateStatus('error', '错误: ' + data.message);
                        setTimeout(() => updateStatus('connected', '已连接'), 3000);
                    }
                } catch (e) {
                    addLog('收到消息: ' + event.data);
                }
            };
        }
        
        function updateStatus(type, text) {
            const statusEl = document.getElementById('status');
            statusEl.textContent = text;
            statusEl.className = 'status ' + type;
        }
        
        function addLog(message) {
            const logEl = document.getElementById('log');
            const timestamp = new Date().toLocaleTimeString();
            logEl.innerHTML += `<div>[${timestamp}] ${message}</div>`;
            logEl.scrollTop = logEl.scrollHeight;
        }
        
        function switchTab(tabName) {
            document.querySelectorAll('.tab-content').forEach(el => {
                el.classList.remove('active');
            });
            document.querySelectorAll('.tab').forEach(el => {
                el.classList.remove('active');
            });
            
            document.getElementById(tabName + '-tab').classList.add('active');
            document.querySelector(`.tab[onclick="switchTab('${tabName}')"]`).classList.add('active');
            currentTab = tabName;
        }
        
        function sendCommand(type, data, params = {}) {
            if (!ws || ws.readyState !== WebSocket.OPEN) {
                addLog('错误: WebSocket未连接');
                return;
            }
            
            const command = {
                type: type,
                data: data,
                params: params
            };
            
            ws.send(JSON.stringify(command));
            addLog('发送命令: ' + type + ' - ' + data);
        }
        
        // 键盘控制函数
        function typeText() {
            const text = document.getElementById('textInput').value;
            if (text) {
                sendCommand('keyboard', text);
                document.getElementById('textInput').value = '';
            }
        }
        
        function sendHotkey(keys) {
            sendCommand('keyboard', keys);
        }
        
        // 命令行函数
        function executeCommand() {
            const cmd = document.getElementById('commandInput').value.trim();
            if (cmd) {
                sendCommand('cmd', cmd, {show_window: false});
                document.getElementById('commandInput').value = '';
            }
        }
        
        function quickCmd(cmd) {
            sendCommand('cmd', cmd, {show_window: false});
        }
        
        // 应用程序函数
        function openApp(appName) {
            sendCommand('app', appName);
        }
        
        // 文件管理函数
        function openPath() {
            const path = document.getElementById('filePath').value;
            if (path) {
                sendCommand('file', path);
                document.getElementById('filePath').value = '';
            }
        }
        
        // 系统控制函数
        function shutdown() {
            if (confirm('确定要关机吗？')) {
                sendCommand('cmd', 'shutdown /s /t 5', {show_window: false});
            }
        }
        
        function restart() {
            if (confirm('确定要重启吗？')) {
                sendCommand('cmd', 'shutdown /r /t 5', {show_window: false});
            }
        }
        
        function lock() {
            sendCommand('cmd', 'rundll32.exe user32.dll,LockWorkStation', {show_window: false});
        }
        
        // 页面加载完成
        window.onload = function() {
            connectWebSocket();
        };
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 设置主机名
  WiFi.setHostname(hostname);
  
  // 连接WiFi
  WiFi.begin(ssid, password);
  Serial.print("正在连接到WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n连接成功!");
  
  // 打印IP信息
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  
  // 启动mDNS
  if (MDNS.begin(hostname)) {
    Serial.println("mDNS启动成功");
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
  }
  
  // 设置Web服务器路由
  server.on("/", handleRoot);
  server.begin();
  
  // 启动WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  Serial.println("服务器启动完成!");
  Serial.println("打开浏览器访问: http://" + WiFi.localIP().toString());
  
  // 清除串口缓冲区
  while (Serial.available()) {
    Serial.read();
  }
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  // 监听串口返回的数据（但不再转发给Windows，避免循环）
  // 这里只读取并显示调试信息，不进行任何转发
  if (Serial.available()) {
    String response = Serial.readStringUntil('\n');
    response.trim();
    
    // 检查是否是Windows的响应
    if (response.startsWith("RESULT:")) {
      // 只打印到串口监视器，不发送给WebSocket，避免循环
      Serial.println("[DEBUG] Windows响应: " + response);
    }
  }
}