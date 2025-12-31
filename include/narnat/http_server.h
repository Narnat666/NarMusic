#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <unistd.h>

class HttpServer {
public:
    HttpServer(int port);
    void start();
    void handleRequest(int clientSocket);
    
private:
    int port_;
    int serverSocket_;
};

#endif // HTTP_SERVER_H