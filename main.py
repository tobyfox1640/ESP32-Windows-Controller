# windows_controller_simple.py
import sys
import os
import json
import time
import serial
import subprocess
import threading
import logging
import ctypes

# 设置日志
def setup_logging():
    log_dir = os.path.join(os.environ['APPDATA'], 'ESP32Controller')
    os.makedirs(log_dir, exist_ok=True)
    log_file = os.path.join(log_dir, 'controller.log')
    
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        handlers=[
            logging.FileHandler(log_file, encoding='utf-8'),
        ]
    )
    return logging.getLogger(__name__)

logger = setup_logging()

# 隐藏控制台窗口
def hide_console():
    try:
        kernel32 = ctypes.WinDLL('kernel32')
        user32 = ctypes.WinDLL('user32')
        hwnd = kernel32.GetConsoleWindow()
        if hwnd:
            user32.ShowWindow(hwnd, 0)
    except:
        pass

class SimpleController:
    def __init__(self):
        self.serial = None
        self.running = True
        self.command_count = 0
    
    def connect_serial(self, com_port=None, baud_rate=115200):
        """连接串口"""
        import serial.tools.list_ports
        
        if com_port:
            ports = [com_port]
        else:
            # 自动检测串口
            ports = [p.device for p in serial.tools.list_ports.comports()
                    if 'USB' in p.description or 'USB-SERIAL' in p.description or '串行设备' in p.description]
        
        for port in ports:
            try:
                self.serial = serial.Serial(port, baud_rate, timeout=1)
                logger.info(f"成功连接到串口: {port}")
                return True
            except Exception as e:
                logger.warning(f"连接串口失败 {port}: {e}")
        
        logger.error("未找到可用的串口")
        return False
    
    def execute_command(self, command_str):
        """执行命令"""
        try:
            # 解析JSON命令
            if command_str.startswith('{'):
                cmd = json.loads(command_str)
                cmd_type = cmd.get('type', 'cmd')
                cmd_data = cmd.get('data', '')
                params = cmd.get('params', {})
            else:
                # 如果不是JSON，直接作为cmd命令
                cmd_type = 'cmd'
                cmd_data = command_str
                params = {}
            
            logger.info(f"执行命令: type={cmd_type}, data={cmd_data}")
            
            if cmd_type == 'keyboard':
                return self.handle_keyboard(cmd_data)
            elif cmd_type == 'cmd':
                return self.handle_command(cmd_data, params)
            elif cmd_type == 'app':
                return self.handle_app(cmd_data)
            elif cmd_type == 'file':
                return self.handle_file(cmd_data)
            else:
                return f"未知命令类型: {cmd_type}"
                
        except Exception as e:
            logger.error(f"命令执行失败: {e}")
            return f"命令执行失败: {str(e)}"
    
    def handle_keyboard(self, data):
        """处理键盘命令"""
        try:
            # 导入pyautogui（如果可用）
            try:
                import pyautogui
                
                if '+' in data:
                    # 组合键，如 ctrl+alt+delete
                    keys = data.split('+')
                    pyautogui.hotkey(*keys)
                    return f"模拟组合键: {data}"
                else:
                    # 单个键或文本
                    if len(data) == 1:
                        pyautogui.press(data)
                        return f"模拟按键: {data}"
                    else:
                        pyautogui.typewrite(data)
                        return f"输入文本: {data}"
            except ImportError:
                # 如果pyautogui不可用，使用备用方法
                if 'win' in data.lower() or 'alt' in data.lower() or 'ctrl' in data.lower():
                    return "需要pyautogui库来模拟组合键，请运行: pip install pyautogui"
                else:
                    # 使用简单的命令替代
                    subprocess.run(f'echo {data}', shell=True, capture_output=True)
                    return f"键盘模拟受限: {data}"
                    
        except Exception as e:
            return f"键盘模拟失败: {str(e)}"
    
    def handle_command(self, cmd, params):
        """执行命令行命令"""
        try:
            show_window = params.get('show_window', False)
            
            startupinfo = subprocess.STARTUPINFO()
            if not show_window:
                startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startupinfo.wShowWindow = subprocess.SW_HIDE
            
            process = subprocess.run(
                cmd,
                shell=True,
                capture_output=True,
                text=True,
                encoding='gbk',
                errors='ignore',
                startupinfo=startupinfo,
                timeout=30
            )
            
            if process.returncode == 0:
                output = process.stdout.strip()
                return f"执行成功: {output[:100] if output else '无输出'}"
            else:
                error = process.stderr.strip()
                return f"执行失败: {error[:100] if error else '未知错误'}"
                
        except subprocess.TimeoutExpired:
            return "命令执行超时"
        except Exception as e:
            return f"命令执行异常: {str(e)}"
    
    def handle_app(self, app_name):
        """打开应用程序"""
        try:
            apps = {
                'notepad': 'notepad.exe',
                'calc': 'calc.exe',
                'cmd': 'cmd.exe',
                'explorer': 'explorer.exe',
                'mspaint': 'mspaint.exe',
            }
            
            if app_name.lower() in apps:
                app_path = apps[app_name.lower()]
            else:
                app_path = app_name
            
            subprocess.Popen(app_path, shell=True, creationflags=subprocess.CREATE_NO_WINDOW)
            return f"已启动应用程序: {app_name}"
            
        except Exception as e:
            return f"启动应用失败: {str(e)}"
    
    def handle_file(self, path):
        """打开文件或文件夹"""
        try:
            if os.path.exists(path):
                os.startfile(path)
                return f"已打开: {path}"
            else:
                subprocess.Popen(f'explorer /select,"{path}"', shell=True, creationflags=subprocess.CREATE_NO_WINDOW)
                return f"尝试打开: {path}"
        except Exception as e:
            return f"打开失败: {str(e)}"
    
    def listen_serial(self):
        """监听串口命令"""
        logger.info("开始监听串口命令...")
        
        while self.running and self.serial and self.serial.is_open:
            try:
                if self.serial.in_waiting > 0:
                    # 读取命令
                    line = self.serial.readline().decode('utf-8', errors='ignore').strip()
                    
                    if line:
                        logger.info(f"收到命令: {line}")
                        self.command_count += 1
                        
                        # 执行命令
                        result = self.execute_command(line)
                        
                        # 发送响应回ESP32
                        response = f"RESULT:{result}"
                        self.serial.write(f"{response}\n".encode('utf-8'))
                        logger.info(f"发送响应: {response}")
                        
            except Exception as e:
                logger.error(f"串口通信错误: {e}")
                time.sleep(1)
    
    def run(self):
        """运行主程序"""
        hide_console()
        logger.info("Windows控制器启动")
        
        # 连接串口
        if not self.connect_serial():
            logger.warning("等待手动连接串口...")
            # 尝试每5秒重连一次
            while self.running and not self.serial:
                try:
                    if self.connect_serial():
                        break
                    time.sleep(5)
                except KeyboardInterrupt:
                    break
        
        # 启动监听线程
        if self.serial:
            thread = threading.Thread(target=self.listen_serial, daemon=True)
            thread.start()
            logger.info("串口监听线程已启动")
        
        # 主循环
        try:
            while self.running:
                time.sleep(1)
        except KeyboardInterrupt:
            self.running = False
            logger.info("程序退出")
        finally:
            if self.serial and self.serial.is_open:
                self.serial.close()
                logger.info("串口已关闭")

def main():
    controller = SimpleController()
    controller.run()

if __name__ == "__main__":
    main()