#include "rate_limiter.h"

namespace narnat {

RateLimiter::RateLimiter() : lastCleanup_(std::chrono::steady_clock::now()) {}

RateLimiter::RateLimiter(const Config& config)
    : config_(config)
    , lastCleanup_(std::chrono::steady_clock::now()) {}

bool RateLimiter::allow(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto it = buckets_.find(key);

    if (it == buckets_.end()) {
        Bucket b;
        b.count = 1;
        b.windowStart = now;
        buckets_[key] = b;
        return true;
    }

    auto& bucket = it->second;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - bucket.windowStart).count();

    if (elapsed >= config_.windowSeconds) {
        bucket.count = 1;
        bucket.windowStart = now;
        return true;
    }

    bucket.count++;
    return bucket.count <= config_.maxRequests;
}

void RateLimiter::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    for (auto it = buckets_.begin(); it != buckets_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.windowStart).count();
        if (elapsed >= config_.windowSeconds * 2) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
    lastCleanup_ = now;
}

} // namespace narnat
