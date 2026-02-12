#监听进程状态，并将信息即时发送给用户
#发送邮件+密钥在users.conf文件
#!/usr/bin/env python3
import smtplib
from email.mime.text import MIMEText
import os
import time
import re
import subprocess
from datetime import datetime
import signal
import sys

# ========== 配置区 ==========
sender = ''
password = ''
receiver = ''
# ============================

# ========== 新增配置项 ==========
log_file_path = '/userdata/Narnat/cpolar/cpolar.log'
address_file_path = '/userdata/Narnat/cpolar/last_address.txt'  # 保存上次地址的文件
status_log_path = '/userdata/Narnat/cpolar/cpolar_status.log'  # 新增：记录进程状态的文件
health_check_interval = 15  # 健康检查间隔，单位：秒
max_log_size_mb = 1  # 最大日志文件大小，单位：MB
# ===============================

# ========== 多用户配置 ==========
users_config_path = '/userdata/Narnat/cpolar/users.conf'  # 用户配置文件路径
users = []  # 将在程序启动时加载，存储所有收件人配置
# ===============================

def load_users():
    """从配置文件加载多个用户的邮箱及授权码"""
    user_list = []
    try:
        with open(users_config_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                parts = line.split(':')
                if len(parts) >= 2:
                    sender_email = parts[0].strip()
                    auth_password = parts[1].strip()
                    receiver_email = parts[2].strip() if len(parts) >= 3 else sender_email
                    user_list.append({
                        'sender': sender_email,
                        'password': auth_password,
                        'receiver': receiver_email
                    })
        if user_list:
            print(f"成功加载 {len(user_list)} 个用户配置")
        else:
            print("用户配置文件为空")
        return user_list
    except FileNotFoundError:
        print(f"用户配置文件 {users_config_path} 不存在，将使用默认单用户")
        return []
    except Exception as e:
        print(f"加载用户配置失败: {e}")
        return []

def log_process_status(event, pid=None, details=""):
    """记录进程状态到单独的日志文件"""
    try:
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        with open(status_log_path, 'a', encoding='utf-8') as f:
            if event == "start":
                pid_info = f" (PID: {pid})" if pid else ""
                f.write(f"[{current_time}] cpolar进程启动{pid_info}{details}\n")
            elif event == "stop":
                pid_info = f" (PID: {pid})" if pid else ""
                f.write(f"[{current_time}] cpolar进程停止{pid_info}{details}\n")
            elif event == "restart":
                pid_info = f" (PID: {pid})" if pid else ""
                f.write(f"[{current_time}] cpolar进程重启{pid_info}{details}\n")
            elif event == "check_alive":
                pid_info = f" (PID: {pid})" if pid else ""
                f.write(f"[{current_time}] cpolar进程存活检查: 正常{pid_info}{details}\n")
            elif event == "check_dead":
                f.write(f"[{current_time}] cpolar进程存活检查: 死亡{details}\n")
        
        print(f"状态已记录到文件: {event}")
    except Exception as e:
        print(f"记录进程状态失败: {e}")

def authenticate_cpolar():
    """执行cpolar认证命令"""
    try:
        # 切换到cpolar目录
        cpolar_dir = '/userdata/Narnat/cpolar'
        os.chdir(cpolar_dir)
        
        # 执行认证命令
        auth_token = 'NzdlZjY2NzItNzhiYi00MjVjLWEyN2QtZWRiOTI0ZDUxOWY2'
        cmd = f'./cpolar authtoken {auth_token}'
        print(f"执行认证命令: {cmd}")
        
        # 执行命令并等待完成
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
        
        if result.returncode == 0:
            print("cpolar认证成功")
            print(f"输出: {result.stdout}")
        else:
            print(f"cpolar认证失败: {result.stderr}")
            return False
            
        return True
    except Exception as e:
        print(f"执行cpolar认证命令时出错: {e}")
        return False

def start_cpolar():
    """启动cpolar进程"""
    try:
        # 检查cpolar是否已经在运行
        result = subprocess.run(['pgrep', '-f', 'cpolar'], capture_output=True, text=True)
        if result.returncode == 0:
            pids = result.stdout.strip().split('\n')
            if pids[0]:
                print(f"cpolar进程已经在运行 (PID: {', '.join(pids)})")
                log_process_status("check_alive", pid=', '.join(pids), details="进程已存在")
                return True
        
        # 切换到cpolar目录
        cpolar_dir = '/userdata/Narnat/cpolar'
        os.chdir(cpolar_dir)
        
        # 启动cpolar命令
        cmd = './cpolar http 8080 -log stdout > cpolar.log 2>&1 &'
        print(f"执行命令: {cmd}")
        
        # 使用shell执行后台命令
        subprocess.Popen(cmd, shell=True, 
                        stdout=subprocess.DEVNULL, 
                        stderr=subprocess.DEVNULL)
        
        print("cpolar进程启动成功")
        
        # 等待2秒让进程完全启动
        time.sleep(2)
        
        # 获取新进程的PID
        result = subprocess.run(['pgrep', '-f', 'cpolar'], capture_output=True, text=True)
        if result.returncode == 0:
            pids = result.stdout.strip().split('\n')
            if pids[0]:
                log_process_status("start", pid=', '.join(pids), details="进程启动成功")
        
        return True
    except Exception as e:
        print(f"启动cpolar进程失败: {e}")
        log_process_status("start", details=f"启动失败: {e}")
        return False

def check_cpolar_process():
    """检查cpolar进程是否存活"""
    try:
        # 使用pgrep检查cpolar进程
        result = subprocess.run(['pgrep', '-f', 'cpolar'], capture_output=True, text=True)
        
        if result.returncode == 0:
            # 进程存在，获取进程ID
            pids = result.stdout.strip().split('\n')
            if pids[0]:  # 确保有进程ID
                # 记录进程存活状态
                log_process_status("check_alive", pid=', '.join(pids), details="进程存活检查")
                return True, pids
            else:
                # 记录进程死亡状态
                log_process_status("check_dead", details="进程不存在")
                return False, []
        else:
            # 进程不存在，记录进程死亡状态
            log_process_status("check_dead", details="进程不存在")
            return False, []
    except Exception as e:
        print(f"检查cpolar进程时出错: {e}")
        log_process_status("check_dead", details=f"检查出错: {e}")
        return False, []

def restart_cpolar():
    """重启cpolar进程"""
    print("尝试重启cpolar进程...")
    
    # 先记录进程死亡
    result = subprocess.run(['pgrep', '-f', 'cpolar'], capture_output=True, text=True)
    if result.returncode == 0:
        pids = result.stdout.strip().split('\n')
        if pids[0]:
            log_process_status("stop", pid=', '.join(pids), details="进程被终止以重启")
    
    # 先尝试杀死所有cpolar进程
    try:
        subprocess.run(['pkill', '-f', 'cpolar'], capture_output=True)
        time.sleep(2)  # 等待进程完全终止
    except Exception as e:
        print(f"终止cpolar进程时出错: {e}")
        log_process_status("stop", details=f"终止失败: {e}")
    
    # 重新启动cpolar
    success = start_cpolar()
    if success:
        log_process_status("restart", details="进程重启成功")
    else:
        log_process_status("restart", details="进程重启失败")
    
    return success

def extract_addresses_from_log():
    """从日志中提取最新的隧道地址"""
    try:
        if not os.path.exists(log_file_path):
            print(f"日志文件不存在: {log_file_path}")
            return []
        
        with open(log_file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except UnicodeDecodeError:
        # 如果utf-8解码失败，尝试其他编码
        try:
            with open(log_file_path, 'r', encoding='gbk') as f:
                content = f.read()
        except:
            with open(log_file_path, 'r', encoding='latin-1') as f:
                content = f.read()
    except Exception as e:
        print(f"读取日志文件失败: {e}")
        return []
    
    # 使用正则表达式匹配隧道地址
    # 匹配格式: Tunnel established at http(s)://xxxxx
    # 修正正则表达式，去掉末尾的双引号
    pattern = r'Tunnel established at (https?://[^\s"]+)'
    matches = re.findall(pattern, content)
    
    # 清理地址：去掉可能的空格和引号
    cleaned_matches = []
    for addr in matches:
        # 去掉末尾的引号、空格等
        cleaned = addr.strip('"\' \t\n\r')
        if cleaned:
            cleaned_matches.append(cleaned)
    
    # 只返回最后一次出现的地址（最后两个）
    return cleaned_matches[-2:] if len(cleaned_matches) >= 2 else cleaned_matches

def save_last_addresses(addresses):
    """保存上一次的地址到文件"""
    try:
        with open(address_file_path, 'w', encoding='utf-8') as f:
            for addr in addresses:
                f.write(addr + '\n')
        print(f"地址已保存到文件: {addresses}")
    except Exception as e:
        print(f"保存地址失败: {e}")

def load_last_addresses():
    """从文件加载上一次的地址"""
    if not os.path.exists(address_file_path):
        return []
    
    try:
        with open(address_file_path, 'r', encoding='utf-8') as f:
            addresses = [line.strip() for line in f.readlines() if line.strip()]
        print(f"从文件加载的地址: {addresses}")
        return addresses
    except Exception as e:
        print(f"加载地址失败: {e}")
        return []

def has_address_changed(new_addresses, old_addresses):
    """检查地址是否发生变化"""
    # 如果两个列表长度不同，肯定有变化
    if len(new_addresses) != len(old_addresses):
        return True
    
    # 比较每个地址（转换为集合比较，忽略顺序）
    return set(new_addresses) != set(old_addresses)

def format_addresses_for_email(addresses):
    """格式化地址用于邮件显示"""
    if not addresses:
        return "无可用地址"
    
    formatted = ""
    for addr in addresses:
        formatted += f"{addr}\n"
    return formatted.strip()

def send_email(addresses):
    """发送地址邮件给所有已配置用户"""
    global users
    if not users:
        print("没有可用的用户配置，邮件发送失败")
        return False
    
    success_count = 0
    current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    addresses_str = format_addresses_for_email(addresses)
    
    for user in users:
        sender_email = user['sender']
        auth_password = user['password']
        receiver_email = user['receiver']
        
        subject = f'Cpolar隧道地址更新 - {current_time}'
        body = f"""
你好，

检测到cpolar隧道地址已更新，最新地址如下：
-----------------------------------------------
{addresses_str}
-----------------------------------------------

更新时间：{current_time}

(此邮件由自动监控脚本发送)
"""
        msg = MIMEText(body, 'plain', 'utf-8')
        msg['From'] = sender_email
        msg['To'] = receiver_email
        msg['Subject'] = subject
        
        try:
            smtp_server = 'smtp.qq.com'
            smtp_port = 587
            server = smtplib.SMTP(smtp_server, smtp_port)
            server.starttls()
            server.login(sender_email, auth_password)
            server.sendmail(sender_email, [receiver_email], msg.as_string())
            server.quit()
            print(f"[{current_time}] 邮件发送成功 -> {receiver_email}")
            success_count += 1
        except Exception as e:
            print(f"[{current_time}] 邮件发送失败 ({receiver_email}): {e}")
    
    return success_count > 0

def send_health_email(status, details=""):
    """发送健康状态邮件给所有已配置用户"""
    global users
    if not users:
        print("没有可用的用户配置，健康邮件发送失败")
        return False
    
    success_count = 0
    current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    if status == "dead":
        subject = f'Cpolar进程异常 - {current_time}'
        body = f"""
你好，

检测到cpolar进程已停止运行，系统已尝试自动重启。

详情: {details}

时间：{current_time}

(此邮件由自动监控脚本发送)
"""
    elif status == "restarted":
        subject = f'Cpolar进程已重启 - {current_time}'
        body = f"""
你好，

cpolar进程已成功重启。

详情: {details}

时间：{current_time}

(此邮件由自动监控脚本发送)
"""
    else:
        return False
    
    for user in users:
        sender_email = user['sender']
        auth_password = user['password']
        receiver_email = user['receiver']
        
        msg = MIMEText(body, 'plain', 'utf-8')
        msg['From'] = sender_email
        msg['To'] = receiver_email
        msg['Subject'] = subject
        
        try:
            smtp_server = 'smtp.qq.com'
            smtp_port = 587
            server = smtplib.SMTP(smtp_server, smtp_port)
            server.starttls()
            server.login(sender_email, auth_password)
            server.sendmail(sender_email, [receiver_email], msg.as_string())
            server.quit()
            print(f"[{current_time}] 健康状态邮件发送成功 -> {receiver_email}")
            success_count += 1
        except Exception as e:
            print(f"[{current_time}] 健康状态邮件发送失败 ({receiver_email}): {e}")
    
    return success_count > 0

def check_and_cleanup_log():
    """检查并清理日志文件，防止过大"""
    try:
        # 检查日志文件是否存在
        if not os.path.exists(log_file_path):
            return True
        
        # 获取日志文件大小
        log_size = os.path.getsize(log_file_path)
        log_size_mb = log_size / (1024 * 1024)
        
        print(f"日志文件大小: {log_size_mb:.2f} MB")
        
        # 如果日志文件超过指定大小，清理日志
        if log_size_mb > max_log_size_mb:
            print(f"日志文件过大({log_size_mb:.2f}MB > {max_log_size_mb}MB)，执行清理...")
            
            # 读取日志文件最后1000行（保留最新内容）
            try:
                with open(log_file_path, 'r', encoding='utf-8') as f:
                    lines = f.readlines()
            except UnicodeDecodeError:
                # 尝试其他编码
                with open(log_file_path, 'r', encoding='gbk') as f:
                    lines = f.readlines()
            
            # 保留最后1000行
            keep_lines = 1000
            if len(lines) > keep_lines:
                kept_lines = lines[-keep_lines:]
                with open(log_file_path, 'w', encoding='utf-8') as f:
                    f.writelines(kept_lines)
                print(f"已清理日志，保留最近{keep_lines}行")
            else:
                # 如果行数不多，但文件大小较大（可能是因为有长行），清空文件
                with open(log_file_path, 'w', encoding='utf-8') as f:
                    f.write("# 日志文件已自动清理\n")
                print("已清空日志文件")
            
            return True
        else:
            return False
    
    except Exception as e:
        print(f"清理日志文件时出错: {e}")
        return False

def cleanup_log():
    """清理日志文件内容，保留文件结构"""
    try:
        if not os.path.exists(log_file_path):
            return False
        
        # 只保留一个标记行，表示日志已清理
        with open(log_file_path, 'w', encoding='utf-8') as f:
            f.write(f"# 日志文件清理于 {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("# 清理原因：地址无变化，减少存储开销\n")
        
        print("日志文件已清理")
        return True
    except Exception as e:
        print(f"清理日志文件失败: {e}")
        return False

def health_check():
    """健康检查：检查cpolar进程是否存活"""
    current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    # 检查cpolar进程
    is_alive, pids = check_cpolar_process()
    
    if is_alive:
        print(f"[{current_time}] 健康检查: cpolar进程正常 (PID: {', '.join(pids)})")
        return True, pids
    else:
        print(f"[{current_time}] 健康检查: cpolar进程已停止")
        
        # 尝试重启
        if restart_cpolar():
            # 等待一段时间让进程完全启动
            time.sleep(3)
            
            # 再次检查是否启动成功
            is_alive_after, new_pids = check_cpolar_process()
            if is_alive_after:
                print(f"[{current_time}] cpolar进程重启成功 (新PID: {', '.join(new_pids)})")
                
                # 发送重启成功通知
                send_health_email("restarted", f"进程重启成功，新PID: {', '.join(new_pids)}")
                return True, new_pids
            else:
                print(f"[{current_time}] cpolar进程重启失败")
                
                # 发送进程异常通知
                send_health_email("dead", "进程重启失败")
                return False, []
        else:
            print(f"[{current_time}] cpolar进程重启失败")
            
            # 发送进程异常通知
            send_health_email("dead", "进程重启失败")
            return False, []

def main():
    """主循环函数"""
    global users
    # ---------- 加载多用户配置 ----------
    users = load_users()
    if not users:
        # 如果没有配置文件或配置为空，使用原来的单个用户
        users = [{'sender': sender, 'password': password, 'receiver': receiver}]
        print(f"未找到用户配置文件或配置为空，使用默认用户: {sender}")
    # ----------------------------------
    
    print("=== Cpolar隧道地址监控程序启动 ===")
    print(f"日志文件: {log_file_path}")
    print(f"状态日志文件: {status_log_path}")
    print(f"健康检查间隔: {health_check_interval}秒")
    print(f"最大日志大小: {max_log_size_mb}MB")
    print(f"已加载用户数量: {len(users)}")
    print("=" * 30)
    
    # 记录程序启动
    log_process_status("start", details="监控程序启动")
    
    # 0. 先等待5秒
    print("等待5秒...")
    time.sleep(5)
    
    # 1. 执行cpolar认证命令
    print("正在执行cpolar认证命令...")
    if not authenticate_cpolar():
        print("警告: cpolar认证失败，程序将继续运行但可能无法正常连接")
    
    # 2. 再等待2秒
    print("等待2秒...")
    time.sleep(2)
    
    # 3. 启动cpolar进程
    print("正在启动cpolar进程...")
    if not start_cpolar():
        print("警告: cpolar启动失败，程序将继续运行但可能无法获取地址")
    
    # 4. 加载上次保存的地址
    last_addresses = load_last_addresses()
    
    # 5. 首次检查并发送地址（如果有）
    print("\n进行首次检查...")
    current_addresses = extract_addresses_from_log()
    
    if current_addresses:
        print(f"首次检查找到地址: {current_addresses}")
        
        # 首次启动，无论如何都发送邮件
        print("首次启动，发送地址邮件...")
        if send_email(current_addresses):
            save_last_addresses(current_addresses)
        last_addresses = current_addresses.copy()
    else:
        print("首次检查未找到地址，等待cpolar启动...")
    
    # 初始化地址无变化计数器
    no_change_count = 0
    check_count = 0
    last_health_check_time = time.time()
    
    # 6. 进入主循环
    print(f"\n进入主循环...")
    
    while True:
        try:
            current_time = time.time()
            check_count += 1
            
            # ========== 健康检查 ==========
            # 每15秒检查一次cpolar进程是否存活
            if current_time - last_health_check_time >= health_check_interval:
                print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 执行健康检查...")
                health_check()
                last_health_check_time = current_time
            # ==============================
            
            # ========== 地址检查 ==========
            # 每30秒检查一次地址（原逻辑保持不变）
            if check_count % 2 == 0:  # 每2次循环检查一次地址（因为健康检查是15秒一次）
                print(f"\n[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] 第 {check_count//2} 次地址检查...")
                
                # 提取当前地址
                current_addresses = extract_addresses_from_log()
                
                # 检查并清理过大的日志文件
                if check_and_cleanup_log():
                    print("检测到日志文件过大，已执行清理")
                
                if current_addresses:
                    print(f"找到地址: {current_addresses}")
                    
                    # 检查地址是否变化
                    if has_address_changed(current_addresses, last_addresses):
                        print("检测到地址变化，发送邮件...")
                        
                        if send_email(current_addresses):
                            save_last_addresses(current_addresses)
                            last_addresses = current_addresses.copy()
                            no_change_count = 0  # 重置无变化计数器
                        else:
                            print("邮件发送失败，保留旧地址")
                    else:
                        no_change_count += 1
                        print(f"地址无变化 (连续{no_change_count}次)")
                        
                        # 如果地址连续3次无变化，清理日志文件
                        if no_change_count >= 3:
                            print("地址连续3次无变化，清理日志文件...")
                            if cleanup_log():
                                no_change_count = 0  # 清理后重置计数器
                                print("日志清理完成")
                else:
                    print("未找到地址")
                    no_change_count = 0  # 重置计数器，因为没找到地址也算是一种变化
            # ==============================
            
            # 等待1秒后继续循环（健康检查和地址检查的间隔由计数器控制）
            time.sleep(1)
            
        except KeyboardInterrupt:
            print("\n\n程序被用户中断")
            log_process_status("stop", details="监控程序被用户中断")
            break
        except Exception as e:
            print(f"发生未预期错误: {e}")
            log_process_status("check_dead", details=f"监控程序出错: {e}")
            time.sleep(1)

if __name__ == "__main__":
    main()