#include "stream_send.h"

StreamSend::StreamSend(const std::string& file_name, long long buff_size) 
: file_name_(file_name), buff_size_(buff_size) {
    buffer_.resize(buff_size);
    file_size_ = get_file_size(file_name);
}

long long StreamSend::get_file_size(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return -1;
    return file.tellg();    
}

bool StreamSend::parse_range_header(const std::string& request) {
    
    std::regex range_regex("Range:\\s*bytes=(\\d+)-(\\d*)", std::regex_constants::icase);
    std::smatch match;

    // 正则表达式，找到对应数据
    if (std::regex_search(request, match, range_regex) && match.size() >= 3) {
        try {

            file_buff_start_ = std::stoll(match[1].str());   // 开始位置
            std::string end_str = match[2].str();
            if (!end_str.empty()) { // 确保不为空
                file_buff_end_ = std::stoll(end_str);
            } else {
                file_buff_end_ = file_size_ - 1;
            }

            // 起始位置不能大于文件大小
            if (file_buff_start_ < 0 || file_buff_start_ >= file_size_) {
                std::cerr << "无效起始位置：" << file_buff_start_ << ",文件大小：" << file_size_ << std::endl;
                return false;
            }

            // 结束位置不能大于文件大小
            if (file_buff_end_ > file_size_) {
                file_buff_end_ = file_size_ - 1;
            }

            if (file_buff_start_ > file_buff_end_) {
                std::cerr << "起始位置大于结束位置：" << file_buff_start_ << ">" << file_buff_end_ << std::endl;
                return false;
            }

            //  限制一次传输数据大小防止服务器崩掉
            long long request_size = file_buff_end_ - file_buff_start_ + 1;
            request_size_ = request_size;
            // 请求大小超过缓冲区大小
            if (request_size > buff_size_) {
                // 调整end为缓冲区大小位置
                file_buff_end_ = file_buff_start_ + buff_size_ - 1;
                // 如果end超过了文件大小
                if (file_buff_end_ >= file_size_) {
                    // end设置为文件末尾
                    file_buff_end_ = file_size_ - 1;
                }
                std::cout << "请求大小 " << request_size << " 超过限制，调整为：" << (file_buff_end_ - file_buff_start_ + 1) << std::endl;
            }
            
            //更新返还数据的大小
            request_size_ = file_buff_end_ - file_buff_start_ + 1;
            std::cout << "start=" << file_buff_start_ << ",end=" << file_buff_end_
                << ",request size=" << request_size_ << std::endl;
            
            return true;

        } catch (const std::exception& e) {
            std::cerr << "解析range失败：" << e.what() << std::endl;
            return false;
        }
    } else { // 未找到
        std::cout << "未找到Range头部，使用默认范围" << std::endl;
        file_buff_start_ = 0;
        std::cout << "buffer size:" << buff_size_ << " file_size: " <<  file_size_ << std::endl;
        file_buff_end_ = std::min(buff_size_, file_size_) - 1;
        request_size_ = file_buff_end_ - file_buff_start_ + 1;
        return false;
    }
}

bool StreamSend::read_data_to_buffer(const std::string& request) { // 读取需要的数据到buffer_
    if (!parse_range_header(request)) return false;

    std::ifstream file(file_name_, std::ios::binary);
    if (!file) {
        std::cerr << "文件打开失败：" << file_name_ << std::endl;
        return false;
    }

    // 定位文件位置并读取
    file.seekg(file_buff_start_, std::ios::beg);
    if (!file.read(buffer_.data(), request_size_)) {
        std::cerr << "读取文件失败" << std::endl;
        return false;
    }

    return true;
}
