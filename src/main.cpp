#include "http_server.h"
#include <iostream>

int main() {
    int port = 8080;
    std::string root_dir = "../www";  // 相对于可执行文件的位置
    
    HttpServer server(port, root_dir);
    
    std::cout << "Starting HTTP server on port " << port << std::endl;
    std::cout << "Web root: " << root_dir << std::endl;
    
    if (!server.start()) {
        std::cerr << "Failed to start server" << std::endl;
        return 1;
    }
    
    return 0;
}