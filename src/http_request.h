#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <map>

class HttpRequest {
public:
    HttpRequest(int socket);
    bool parse();
    std::string getMethod() const;
    std::string getPath() const;
    std::string getHeader(const std::string& key) const;
    std::string getBody() const;
    int getBodyLength() const;

private:
    int socket_;
    std::string method_;
    std::string path_;
    std::map<std::string, std::string> headers_;
    std::string body_;
    int body_length_;
};

#endif // HTTP_REQUEST_H