#!/bin/sh
#
# 开机自启动服务器脚本，功能为设置wifi ip、利用service脚本启动NarMusic程序
# 脚本需要放到/etc/init.d目录下做自启动
# S99narnat 启动等级为S99最后启动
#

NarMusic_IP=192.168.2.212

case "$1" in
  start)
    {
      echo "=== Service Start ==="
      echo "Timestamp: $(date '+%Y-%m-%d %H:%M:%S')"
      echo "Boot Time: $(uptime)"
      echo ""

      echo "Waiting 5 seconds..."
      sleep 5

      echo "Configuring wlan0 IP to ${NarMusic_IP}"
      /sbin/ifconfig wlan0 "${NarMusic_IP}" netmask 255.255.255.0 up

      echo "Waiting 5 seconds..."
      sleep 5

      echo "Starting NarMusic..."
      service start NarMusic

      echo ""
      echo "Started at: $(date '+%Y-%m-%d %H:%M:%S')"
      echo "=== Done ==="
    } > /root/start.log 2>&1
    ;;

  stop)
    service stop NarMusic 2>/dev/null
    echo "Stopped at: $(date '+%Y-%m-%d %H:%M:%S')" > /root/start.log 2>&1
    ;;

  restart)
    $0 stop
    sleep 1
    $0 start
    ;;

  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac

exit 0