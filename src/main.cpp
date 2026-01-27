#include "http_server.h"
#include <iostream>
#include <unistd.h>
#include <string>
#include "curl/curl.h"
#include <signal.h>

std::string path = "";
std::string ext = "";
int port = 8080;
bool debug = false;


int main(int argc, char* argv[]) {

    // 忽略SIGPIPE信号
    signal(SIGPIPE, SIG_IGN);

    // 初始化 curl 全局资源（在程序最早阶段调用）
    CURLcode curlInit = curl_global_init(CURL_GLOBAL_ALL);
    if (curlInit != CURLE_OK) {
        std::cerr << "curl全局初始化失败！" << std::endl;
        return 1;
    }
    
    // 注册退出时的清理函数（确保任何方式退出都能清理）
    atexit([]() {
        curl_global_cleanup();
        std::cout << "curl全局资源已清理" << std::endl;
    });

    int o;

    // 有参情况下
    if (argc > 1) {
        while ((o = getopt(argc, argv, "o:p:e:dh")) != -1) {
            if (o == 'p') path = optarg;
            else if (o == 'e') ext = optarg;
            else if (o == 'd') debug = true;
            else if (o == 'o') {
                std::string port_string = optarg;
                try {
                    port = std::stoi(port_string);
                }
                catch(const std::exception& e) {
                    std::cerr << "无效参数：" << e.what() << '\n';
                }
                
            }
            else if (o == 'h') {
                std::cout << "用法：" << argv[0] << " -p <path> -e <extension> -o <port> -d" << std::endl;
                return 0;
            } else {
                std::cout << "参数错误可通过 " << argv[0] << "-h" << " 获取参数信息" << std::endl;
                return -1;
            }
        }
        std::cout << "\n文件保存位置为：" << path << " 文件保存格式为：" << ext << " 绑定端口为：" << port << " 调试模式状态为：" << (debug ? "开启\n" : "关闭\n") << std::endl;
    }

    {
    HttpServer server(port);
    server.start();
    }
    return 0;
}