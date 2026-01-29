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
HTTP_SERVER_ARGS="-p ./download/ -e .m4a" # http_server启动参数
HTTP_SERVER_PID_FILE="./logs/http_server.pid" # 存储http_server进程PID的文件
DAEMON_PID_FILE="./logs/service_daemon.pid"  # 存储守护进程PID的文件
HTTP_SERVER_LOG="./logs/http_server.log"     # http_server进程日志
SERVICE_LOG="./logs/service.log"             # 服务管理日志
MONITOR_INTERVAL=10                           # 监控间隔（秒）

# 创建日志目录（如果不存在）
create_log_dir() {
    if [ ! -d "./logs" ]; then
        mkdir -p "./logs"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 创建日志目录: ./logs" >> "$SERVICE_LOG"
    fi
}

# 备份日志文件
backup_logs() {
    local timestamp=$(date '+%Y%m%d_%H%M%S')
    
    # 备份http_server.log（如果存在）
    if [ -f "$HTTP_SERVER_LOG" ]; then
        mv "$HTTP_SERVER_LOG" "${HTTP_SERVER_LOG}.bak_${timestamp}"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 已备份http_server日志: ${HTTP_SERVER_LOG}.bak_${timestamp}" >> "$SERVICE_LOG"
    fi
    
    # 备份service.log（如果存在且非空）
    if [ -f "$SERVICE_LOG" ] && [ -s "$SERVICE_LOG" ]; then
        mv "$SERVICE_LOG" "${SERVICE_LOG}.bak_${timestamp}"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 已备份service日志: ${SERVICE_LOG}.bak_${timestamp}" > "$SERVICE_LOG"
    fi
}

# 启动http_server进程
start_http_server() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动http_server进程..." >> "$SERVICE_LOG"
    
    # 执行http_server命令，将输出重定向到日志文件
    # 使用nohup和&让进程在后台运行
    nohup $HTTP_SERVER_CMD $HTTP_SERVER_ARGS >> "$HTTP_SERVER_LOG" 2>&1 &
    
    # 获取并保存进程PID
    local pid=$!
    echo $pid > "$HTTP_SERVER_PID_FILE"
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server已启动，PID: $pid" >> "$SERVICE_LOG"
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动命令: $HTTP_SERVER_CMD $HTTP_SERVER_ARGS" >> "$SERVICE_LOG"
    
    return $pid
}

# 检查进程是否存活
check_process_alive() {
    local pid=$1
    if [ -z "$pid" ] || [ "$pid" -le 0 ]; then
        return 1  # PID无效，进程不存在
    fi
    
    # 检查/proc目录中是否存在该PID的进程信息
    if [ -d "/proc/$pid" ]; then
        return 0  # 进程存活
    else
        return 1  # 进程不存在
    fi
}

# 守护进程函数 - 监控http_server进程
monitor_http_server() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - 启动守护进程监控，PID: $$" >> "$SERVICE_LOG"
    echo $$ > "$DAEMON_PID_FILE"
    
    local restart_count=0
    local current_pid=""
    
    # 主监控循环
    while true; do
        # 检查PID文件是否存在
        if [ -f "$HTTP_SERVER_PID_FILE" ]; then
            current_pid=$(cat "$HTTP_SERVER_PID_FILE")
            
            # 检查进程是否存活
            if check_process_alive "$current_pid"; then
                # 进程正常，重置重启计数
                restart_count=0
            else
                # 进程已死，重启
                restart_count=$((restart_count + 1))
                echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server进程(PID: $current_pid)已停止，正在重启(重启次数: $restart_count)..." >> "$SERVICE_LOG"
                
                # 启动新的http_server进程
                start_http_server
                current_pid=$?
                
                # 写入重启日志
                echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server重启完成，新PID: $current_pid" >> "$SERVICE_LOG"
            fi
        else
            # PID文件不存在，尝试启动http_server
            echo "$(date '+%Y-%m-%d %H:%M:%S') - 未找到PID文件，启动http_server..." >> "$SERVICE_LOG"
            start_http_server
            current_pid=$?
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
    
    # 检查是否已经在运行
    if [ -f "$DAEMON_PID_FILE" ] && check_process_alive $(cat "$DAEMON_PID_FILE"); then
        echo "服务已经在运行中，守护进程PID: $(cat "$DAEMON_PID_FILE")"
        echo "$(date '+%Y-%m-%d %H:%M:%S') - 尝试重复启动服务，当前守护进程PID: $(cat "$DAEMON_PID_FILE")" >> "$SERVICE_LOG"
        exit 1
    fi
    
    # 启动http_server
    start_http_server
    local http_pid=$?
    
    # 启动守护进程（在后台运行）
    monitor_http_server &
    
    echo "http_server已启动，PID: $http_pid"
    echo "守护进程已启动，PID: $!"
    echo "日志文件:"
    echo "  - http_server日志: $HTTP_SERVER_LOG"
    echo "  - 服务日志: $SERVICE_LOG"
}

# 停止服务
stop_service() {
    # 停止守护进程
    if [ -f "$DAEMON_PID_FILE" ]; then
        local daemon_pid=$(cat "$DAEMON_PID_FILE")
        
        if check_process_alive "$daemon_pid"; then
            echo "停止守护进程(PID: $daemon_pid)..."
            kill "$daemon_pid"
            
            # 等待进程结束
            sleep 1
            
            # 检查是否成功停止
            if check_process_alive "$daemon_pid"; then
                echo "警告: 守护进程仍在运行，尝试强制停止..."
                kill -9 "$daemon_pid" 2>/dev/null
            fi
            
            echo "$(date '+%Y-%m-%d %H:%M:%S') - 守护进程已停止，PID: $daemon_pid" >> "$SERVICE_LOG"
        fi
        
        # 删除PID文件
        rm -f "$DAEMON_PID_FILE"
    fi
    
    # 停止http_server进程
    if [ -f "$HTTP_SERVER_PID_FILE" ]; then
        local http_pid=$(cat "$HTTP_SERVER_PID_FILE")
        
        if check_process_alive "$http_pid"; then
            echo "停止http_server进程(PID: $http_pid)..."
            kill "$http_pid"
            
            # 等待进程结束
            sleep 2
            
            # 检查是否成功停止
            if check_process_alive "$http_pid"; then
                echo "警告: http_server进程仍在运行，尝试强制停止..."
                kill -9 "$http_pid" 2>/dev/null
            fi
            
            echo "$(date '+%Y-%m-%d %H:%M:%S') - http_server进程已停止，PID: $http_pid" >> "$SERVICE_LOG"
        fi
        
        # 删除PID文件
        rm -f "$HTTP_SERVER_PID_FILE"
    fi
    
    echo "服务已停止"
}

# 显示帮助信息
show_help() {
    echo "服务管理脚本使用方法:"
    echo "  service start http_server   启动http_server服务并启用进程监控"
    echo "  service stop http_server    停止http_server服务及进程监控"
    echo "  service help                显示此帮助信息"
    echo ""
    echo "说明:"
    echo "  1. 启动服务时会自动备份现有日志"
    echo "  2. 守护进程每10秒检查一次http_server进程状态"
    echo "  3. 如果进程异常退出，会自动重启并记录到service.log"
    echo "  4. http_server进程日志输出到: ./logs/http_server.log"
    echo "  5. 服务管理日志输出到: ./logs/service.log"
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