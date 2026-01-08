#ifndef STREAM_SEND_H
#define STREAM_SEND_H

#include <fstream>
#include <vector>
#include <regex>
#include <iostream>

class StreamSend {
    public:
        StreamSend(const std::string& file_name, long long buff_size);
        long long get_file_size(const std::string& filename); // 获取文件大小函数
        long long request_size_;
        bool read_data_to_buffer(const std::string& request);
        std::vector<char> buffer_;   // 二进制存储容器
        long long file_size_; // 保存文件大小
        long long file_buff_start_;  // buff 存储的文件其实于文件哪里
        long long file_buff_end_;    // buff 存储的文件结束于文件的哪里

    private:
        std::string file_name_;     // 文件名
        long long buff_size_;   // 缓冲区大小
        bool parse_range_header(const std::string& request); // 从请求协议中解析范围
};

// 流式传输类,需要返还读取的数据buff、start、end、file_size、chunk_size

#endif