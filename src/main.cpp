#include "http_server.h"
#include <iostream>

int main() {
    HttpServer server(8080);
    server.start();
    return 0;
}