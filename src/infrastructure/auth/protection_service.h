#ifndef NARNAT_PROTECTION_SERVICE_H
#define NARNAT_PROTECTION_SERVICE_H

#include <string>
#include <unordered_set>
#include <mutex>
#include <random>

namespace narnat {

class ProtectionService {
public:
    explicit ProtectionService(const std::string& password)
        : password_(password), rng_(std::random_device{}()) {}

    bool isEnabled() const { return !password_.empty(); }

    bool verifyPassword(const std::string& input) const {
        return input == password_;
    }

    std::string generateToken() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string token = randomHex(32);
        tokens_.insert(token);
        return token;
    }

    bool validateToken(const std::string& token) {
        if (token.empty()) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return tokens_.count(token) > 0;
    }

    void revokeToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        tokens_.erase(token);
    }

private:
    std::string randomHex(size_t length) {
        static const char hex[] = "0123456789abcdef";
        std::uniform_int_distribution<int> dist(0, 15);
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; i++) {
            result += hex[dist(rng_)];
        }
        return result;
    }

    std::string password_;
    std::unordered_set<std::string> tokens_;
    std::mutex mutex_;
    std::mt19937 rng_;
};

} // namespace narnat

#endif
