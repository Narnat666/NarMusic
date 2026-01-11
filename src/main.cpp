#include "http_server.h"
#include <iostream>
#include <unistd.h>
#include <string>

std::string path = "";
std::string ext = "";
int port = 8080;
bool debug = false;


int main(int argc, char* argv[]) {
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

    HttpServer server(port);
    server.start();
    return 0;
}