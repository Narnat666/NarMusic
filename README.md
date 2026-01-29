# http_server

交叉编译：
mkdir build-aarch64
cd build-aarch64
cmake -D CMAKE_TOOLCHAIN_FILE=../toolchain-aarch64.cmake ..
make

# 学习目标和成果

1. cmake的基础语法了解以及基础的项目构建 [完成]
2. 交叉编译器配合cmake编译c++项目 [完成]
3. 了解脚本基础语法和使用脚本 [完成]
4. 巩固c++知识 [正在进行]
5. 线程池管理线程 [完成]
6. 优化网络协议代码、利用buildroot编译可能的目标架构依赖库 [大部分完成]
7. 交叉编译curl库利用curl库函数和nlohmann开源库优化项目代码，链接依赖库 [完成]
8. 深刻了解并解决交叉编译器版本不匹配导致的许多链接问题 [完成]
9. 静态库交叉编译项目 [待完成]
10. 音频流媒体技术了解和学习，将音频像流水一样发送给客户端 [待完成]
11. 代码整合，更进一步理解c++面向对象编程核心 [待完成]
12. 移值github音乐文件写入库 [完成]
13. 音乐文件支持歌词写入、封面写入、歌词与音频对齐功能 [完成]

...

# 共享文件夹挂载
1.查看可挂载共享文件夹 
vmware-hgfsclient
2.将windows下的文件挂载到ubantu
vmhgfs-fuse .host:/tanran /mnt/hgfs/tanran/ -o allow_other,uid=$(id -u),gid=$(id -g)

# 程序完全静默运行
(./http_server -p ./download/ -e .m4a > http_server.log 2>&1 &)