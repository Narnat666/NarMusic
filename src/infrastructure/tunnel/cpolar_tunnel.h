#ifndef NARNAT_CPOLAR_TUNNEL_H
#define NARNAT_CPOLAR_TUNNEL_H

#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include "config/config.h"

namespace narnat {

class CurlClient;

class CpolarTunnel {
public:
    explicit CpolarTunnel(const CpolarConfig& config, int localPort);
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
    std::string cleanUrl(const std::string& url);

    std::string resolveExeDir();

    CpolarConfig config_;
    int localPort_;
    std::atomic<bool> running_{false};
    std::string publicUrl_;
    std::string resolvedBinPath_;
    std::unique_ptr<CurlClient> apiClient_;
    std::thread monitorThread_;
};

} // namespace narnat

#endif
