#ifndef NARNAT_EMAIL_SENDER_H
#define NARNAT_EMAIL_SENDER_H

#include <string>
#include "config/config.h"

namespace narnat {

class EmailSender {
public:
    // 单账户发送
    static bool sendTo(const EmailConfig& config, const EmailAccount& account,
                       const std::string& subject, const std::string& body);

    // 遍历所有 accounts 逐个发送
    static bool sendAll(const EmailConfig& config,
                        const std::string& subject, const std::string& body);
};

} // namespace narnat

#endif
