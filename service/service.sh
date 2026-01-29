#!/bin/bash

# =============================================
# 服务管理脚本：service.sh
# 功能：
#   1. service start http_server - 启动http_server并监控进程
#   2. service stop http_server - 停止http_server及监控
#   3. service help - 显示帮助信息
# =============================================

# 全局变量定义
HTTP_SERVER_CMD="/userdata/Narnat/http_server"          # http_server可执行文件路径
HTTP_SERVER_ARGS="-p ./download/ -e .m4a"              # http_server启动参数
HTTP_SERVER_DIR="/userdata/Narnat"                      # http_server所在目录
HTTP_SERVER_PID_FILE="/userdata/Narnat/logs/http_server.pid" # 存储http_server进程PID的文件
DAEMON_PID_FILE="/userdata/Narnat/logs/service_daemon.pid"  # 存储守护进程PID的文件
HTTP_SERVER_LOG="/userdata/Narnat/logs/http_server.log"     # http_server进程日志
SERVICE_LOG="/userdata/Narnat/logs/service.log"             # 服务管理日志
MONITOR_INTERVAL=5                                        # 监控间隔（秒）
MAX_RESTARTS=10                                          # 最大重启次数（防止无限重启）

# 创建日志目录（如果不存在）
create_log_dir() {
    if [ ! -d "/userdata/Narnat/logs" ]; then
        mkdir -p "/userdata/Narnat/logs"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 创建日志目录: /userdata/Narnat/logs" >> "$SERVICE_LOG"
    fi
}

# 备份日志文件
backup_logs() {
    # 备份http_server.log（如果存在）
    if [ -f "$HTTP_SERVER_LOG" ]; then
        mv "$HTTP_SERVER_LOG" "${HTTP_SERVER_LOG}.bak"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 已备份http_server日志: ${HTTP_SERVER_LOG}.bak" >> "$SERVICE_LOG"
    fi
    
    # 备份service.log（如果存在且非空）
    if [ -f "$SERVICE_LOG" ] && [ -s "$SERVICE_LOG" ]; then
        mv "$SERVICE_LOG" "${SERVICE_LOG}.bak"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 已备份service日志: ${SERVICE_LOG}.bak" > "$SERVICE_LOG"
    fi
}

# 启动http_server进程
start_http_server() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动http_server进程..." >> "$SERVICE_LOG"
    
    # 切换到http_server所在目录
    cd "$HTTP_SERVER_DIR" || {
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 错误: 无法切换到目录 $HTTP_SERVER_DIR" >> "$SERVICE_LOG"
        return 1
    }
    
    # 检查是否已有进程在运行
    if [ -f "$HTTP_SERVER_PID_FILE" ]; then
        local old_pid=$(cat "$HTTP_SERVER_PID_FILE")
        if [ -n "$old_pid" ] && [ -d "/proc/$old_pid" ]; then
            echo "$(date '+%Y-%m-%d %H:%M:%S') - 警告: 已有http_server进程在运行(PID: $old_pid)，先停止它" >> "$SERVICE_LOG"
            kill -9 "$old_pid" 2>/dev/null
            sleep 1
        fi
    fi
    
    # 执行http_server命令，将输出重定向到日志文件
    nohup ./http_server $HTTP_SERVER_ARGS >> "$HTTP_SERVER_LOG" 2>&1 &
    
    # 获取并保存进程PID
    local pid=$!
    echo $pid > "$HTTP_SERVER_PID_FILE"
    
    # 等待1秒确认进程是否启动成功
    sleep 1
    if ! check_process_alive "$pid"; then
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 错误: http_server进程启动后立即退出，PID: $pid" >> "$SERVICE_LOG"
        rm -f "$HTTP_SERVER_PID_FILE"
        return 1
    fi
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server已启动，PID: $pid" >> "$SERVICE_LOG"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动命令: ./http_server $HTTP_SERVER_ARGS (在目录: $HTTP_SERVER_DIR)" >> "$SERVICE_LOG"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 当前工作目录: $(pwd)" >> "$SERVICE_LOG"
    
    return 0
}

# 检查进程是否存活
check_process_alive() {
    local pid=$1
    if [ -z "$pid" ] || [ "$pid" -le 0 ]; then
        return 1  # PID无效，进程不存在
    fi
    
    # 检查/proc目录中是否存在该PID的进程信息
    if [ -d "/proc/$pid" ]; then
        # 进一步检查进程名是否包含http_server
        local process_name=$(cat "/proc/$pid/comm" 2>/dev/null)
        if [[ "$process_name" == *"http_server"* ]] || ps -p "$pid" > /dev/null 2>&1; then
            return 0  # 进程存活
        fi
    fi
    return 1  # 进程不存在
}

# 守护进程函数 - 监控http_server进程
monitor_http_server() {
    # 注意：不再在这里写入PID文件，改为在start_service中用$!获取正确PID
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动守护进程监控，PID: $BASHPID" >> "$SERVICE_LOG"
    
    local restart_count=0
    local last_pid=""
    
    # 主监控循环
    while true; do
        # 检查PID文件是否存在
        if [ -f "$HTTP_SERVER_PID_FILE" ]; then
            local current_pid=$(cat "$HTTP_SERVER_PID_FILE" 2>/dev/null)
            
            # 检查进程是否存活
            if check_process_alive "$current_pid"; then
                # 进程正常，重置重启计数
                if [ "$current_pid" != "$last_pid" ]; then
                    restart_count=0
                    last_pid="$current_pid"
                fi
            else
                # 进程已死，检查是否超过最大重启次数
                if [ $restart_count -ge $MAX_RESTARTS ]; then
                    echo "$(date '+%Y-%m-%d %H:%M:%S') - 错误: 达到最大重启次数($MAX_RESTARTS)，停止重启" >> "$SERVICE_LOG"
                    rm -f "$HTTP_SERVER_PID_FILE"
                    exit 1
                fi
                
                restart_count=$((restart_count + 1))
                echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server进程(PID: $current_pid)已停止，正在重启(重启次数: $restart_count)..." >> "$SERVICE_LOG"
                
                # 启动新的http_server进程
                if start_http_server; then
                    current_pid=$(cat "$HTTP_SERVER_PID_FILE" 2>/dev/null)
                    echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server重启完成，新PID: $current_pid" >> "$SERVICE_LOG"
                else
                    echo "$(date '+%Y-%m-%d %H:%M:%S') - 错误: 无法重启http_server" >> "$SERVICE_LOG"
                fi
            fi
        else
            # PID文件不存在，尝试启动http_server
            echo "$(date '+%Y-%m-%d %H:%M:%S') - 未找到PID文件，启动http_server..." >> "$SERVICE_LOG"
            if start_http_server; then
                restart_count=0
            fi
        fi
        
        # 等待指定的监控间隔
        sleep $MONITOR_INTERVAL
    done
}

# 启动服务
start_service() {
    # 检查http_server可执行文件是否存在
    if [ ! -f "$HTTP_SERVER_CMD" ]; then
        echo "错误: 找不到http_server可执行文件: $HTTP_SERVER_CMD"
        exit 1
    fi
    
    # 创建日志目录
    create_log_dir
    
    # 备份日志
    backup_logs
    
    # 检查并清理残留的守护进程（如果存在）
    if [ -f "$DAEMON_PID_FILE" ]; then
        local old_daemon_pid=$(cat "$DAEMON_PID_FILE" 2>/dev/null)
        if [ -n "$old_daemon_pid" ] && check_process_alive "$old_daemon_pid"; then
            echo "发现残留守护进程(PID: $old_daemon_pid)，正在停止..."
            kill -9 "$old_daemon_pid" 2>/dev/null
            sleep 2
        fi
        rm -f "$DAEMON_PID_FILE" 2>/dev/null
    fi
    
    # 检查并清理残留的http_server进程（如果存在）
    if [ -f "$HTTP_SERVER_PID_FILE" ]; then
        local old_http_pid=$(cat "$HTTP_SERVER_PID_FILE" 2>/dev/null)
        if [ -n "$old_http_pid" ] && check_process_alive "$old_http_pid"; then
            echo "发现残留http_server进程(PID: $old_http_pid)，正在停止..."
            kill -9 "$old_http_pid" 2>/dev/null
            sleep 2
        fi
        rm -f "$HTTP_SERVER_PID_FILE" 2>/dev/null
    fi
    
    # 清理旧的PID文件（确保万无一失）
    rm -f "$HTTP_SERVER_PID_FILE" "$DAEMON_PID_FILE" 2>/dev/null
    
    # 启动守护进程（在后台运行）
    monitor_http_server &
    local daemon_pid=$!
    
    # 关键修复：使用$!获取正确的后台进程PID并写入文件
    echo $daemon_pid > "$DAEMON_PID_FILE"
    
    # 等待守护进程启动
    sleep 2
    
    echo "服务已启动"
    echo "守护进程PID: $daemon_pid"
    echo "日志文件:"
    echo "  - http_server日志: $HTTP_SERVER_LOG"
    echo "  - 服务日志: $SERVICE_LOG"
    echo "工作目录: $HTTP_SERVER_DIR"
    
    # 等待守护进程写入PID文件（实际上已经写入，这里保留原逻辑但显示的是正确的PID）
    sleep 1
    if [ -f "$DAEMON_PID_FILE" ]; then
        echo "守护进程PID（从文件）: $(cat "$DAEMON_PID_FILE")"
    fi
}

# 停止服务
stop_service() {
    echo "停止服务..."
    
    # 先停止守护进程（防止它在停止http_server时重启它）
    if [ -f "$DAEMON_PID_FILE" ]; then
        local daemon_pid=$(cat "$DAEMON_PID_FILE" 2>/dev/null)
        
        if [ -n "$daemon_pid" ] && check_process_alive "$daemon_pid"; then
            echo "停止守护进程(PID: $daemon_pid)..."
            kill "$daemon_pid" 2>/dev/null
            
            # 等待进程结束
            sleep 2
            
            # 检查是否成功停止，若未停止则强制杀死
            if check_process_alive "$daemon_pid"; then
                echo "强制停止守护进程..."
                kill -9 "$daemon_pid" 2>/dev/null
            fi
            
            # 再次确认守护进程已死（防止race condition）
            sleep 1
            if check_process_alive "$daemon_pid"; then
                echo "警告: 守护进程未能停止(PID: $daemon_pid)"
            else
                echo "$(date '+%Y-%m-%d %H:%M:%S') - 守护进程已停止，PID: $daemon_pid" >> "$SERVICE_LOG"
            fi
        fi
        
        # 删除PID文件
        rm -f "$DAEMON_PID_FILE" 2>/dev/null
    fi
    
    # 停止http_server进程（此时守护进程已死，不会重启）
    if [ -f "$HTTP_SERVER_PID_FILE" ]; then
        local http_pid=$(cat "$HTTP_SERVER_PID_FILE" 2>/dev/null)
        
        if [ -n "$http_pid" ] && check_process_alive "$http_pid"; then
            echo "停止http_server进程(PID: $http_pid)..."
            kill "$http_pid" 2>/dev/null
            
            # 等待进程结束
            sleep 2
            
            # 检查是否成功停止
            if check_process_alive "$http_pid"; then
                echo "强制停止http_server进程..."
                kill -9 "$http_pid" 2>/dev/null
            fi
            
            echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server进程已停止，PID: $http_pid" >> "$SERVICE_LOG"
        fi
        
        # 删除PID文件
        rm -f "$HTTP_SERVER_PID_FILE" 2>/dev/null
    fi
    
    echo "服务已停止"
}

# 显示服务状态
show_status() {
    echo "服务状态:"
    
    if [ -f "$DAEMON_PID_FILE" ]; then
        local daemon_pid=$(cat "$DAEMON_PID_FILE" 2>/dev/null)
        if [ -n "$daemon_pid" ] && check_process_alive "$daemon_pid"; then
            echo "  守护进程: 运行中 (PID: $daemon_pid)"
        else
            echo "  守护进程: 已停止"
        fi
    else
        echo "  守护进程: 未运行"
    fi
    
    if [ -f "$HTTP_SERVER_PID_FILE" ]; then
        local http_pid=$(cat "$HTTP_SERVER_PID_FILE" 2>/dev/null)
        if [ -n "$http_pid" ] && check_process_alive "$http_pid"; then
            echo "  http_server: 运行中 (PID: $http_pid)"
        else
            echo "  http_server: 已停止"
        fi
    else
        echo "  http_server: 未运行"
    fi
    
    if [ -f "$SERVICE_LOG" ]; then
        echo "  服务日志: $SERVICE_LOG"
        echo "  最后几条日志:"
        tail -5 "$SERVICE_LOG" 2>/dev/null | sed 's/^/    /'
    fi
}

# 显示帮助信息
show_help() {
    echo "服务管理脚本使用方法:"
    echo "  service start http_server   启动http_server服务并启用进程监控"
    echo "  service stop http_server    停止http_server服务及进程监控"
    echo "  service status              显示服务状态"
    echo "  service help                显示此帮助信息"
    echo ""
    echo "说明:"
    echo "  1. 启动服务时会自动备份现有日志"
    echo "  2. 守护进程每5秒检查一次http_server进程状态"
    echo "  3. 最大重启次数限制为10次，防止无限重启"
    echo "  4. http_server进程日志输出到: /userdata/Narnat/logs/http_server.log"
    echo "  5. 服务管理日志输出到: /userdata/Narnat/logs/service.log"
    echo "  6. http_server会在其所在目录(/userdata/Narnat)下启动"
}

# 主函数
main() {
    # 参数检查
    if [ $# -lt 1 ]; then
        echo "错误: 参数不足"
        show_help
        exit 1
    fi
    
    case "$1" in
        "start")
            if [ "$2" = "http_server" ]; then
                start_service
            else
                echo "错误: 未知的服务名称 '$2'"
                echo "目前仅支持: http_server"
                exit 1
            fi
            ;;
        "stop")
            if [ "$2" = "http_server" ]; then
                stop_service
            else
                echo "错误: 未知的服务名称 '$2'"
                echo "目前仅支持: http_server"
                exit 1
            fi
            ;;
        "status")
            show_status
            ;;
        "help")
            show_help
            ;;
        *)
            echo "错误: 未知的命令 '$1'"
            show_help
            exit 1
            ;;
    esac
}

# 脚本入口点
if [ "$0" = "$BASH_SOURCE" ]; then
    main "$@"
fi