#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <map>

class HttpRequest {
public:
    HttpRequest();
    
    void parse(const std::string& request);
    void print() const;
    
    std::string getMethod() const;
    std::string getPath() const;
    std::string getHeader(const std::string& key) const;

private:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
};

#endif