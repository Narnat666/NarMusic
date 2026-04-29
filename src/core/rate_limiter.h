#ifndef NARNAT_RATE_LIMITER_H
#define NARNAT_RATE_LIMITER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace narnat {

class RateLimiter {
public:
    struct Config {
        int maxRequests = 60;
        int windowSeconds = 60;
    };

    RateLimiter();
    explicit RateLimiter(const Config& config);

    bool allow(const std::string& key);

    void cleanup();

private:
    struct Bucket {
        int count = 0;
        std::chrono::steady_clock::time_point windowStart;
    };

    Config config_;
    std::unordered_map<std::string, Bucket> buckets_;
    std::mutex mutex_;
    std::chrono::steady_clock::time_point lastCleanup_;
};

} // namespace narnat

#endif
