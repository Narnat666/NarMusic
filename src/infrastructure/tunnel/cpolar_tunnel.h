#ifndef NARNAT_CPOLAR_TUNNEL_H
#define NARNAT_CPOLAR_TUNNEL_H

#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include "config/config.h"

namespace narnat {

class CurlClient;

class CpolarTunnel {
public:
    explicit CpolarTunnel(const CpolarConfig& cpolarConfig,
                          const EmailConfig& emailConfig,
                          int localPort);
    ~CpolarTunnel();

    bool start();
    void stop();

    bool isRunning() const { return running_; }
    std::string publicUrl() const { return publicUrl_; }

private:
    bool resolveBinary();
    bool checkBinary();
    bool configureAuthtoken();
    bool launchAndCapture();
    std::string fetchUrlFromApi();
    std::string fetchUrlFromDashboard();
    void printBanner();
    void monitorLoop();
    void notifyUrl(const std::string& url, bool changed);
    std::string cleanUrl(const std::string& url);

    std::string resolveExeDir();

    CpolarConfig cpolarConfig_;
    EmailConfig emailConfig_;
    int localPort_;
    std::atomic<bool> running_{false};
    std::string publicUrl_;
    std::string resolvedBinPath_;
    std::unique_ptr<CurlClient> apiClient_;
    std::thread monitorThread_;
};

} // namespace narnat

#endif
