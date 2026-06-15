# 指定 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(ShortVideoRank)

# 设置 C++ 标准为 C++14
# 因为你使用了智能指针 (shared_ptr) 和 lambda 表达式，C++14 是必须的
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译生成的二进制可执行文件的输出路径
# 你的可执行文件会生成到项目根目录下的 bin 文件夹中
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 指定头文件的搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include   # 你自己的 .h 文件夹
    ${PROJECT_SOURCE_DIR}/include/muduo
    ${PROJECT_SOURCE_DIR}/include/database
    /usr/include/mysql              # MySQL 官方库的头文件路径
)
# 自动搜索源文件（报错的根源就在这）
# GLOB_RECURSE 会递归扫描 src 目录下所有的 .cpp 文件并赋值给 SOURCES 变量
# 注意：当你新增或删除文件时，CMake 不会自动感知，必须重新运行 cmake 命令
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)
# 手动
# set(SOURCES
#     src/Connection.cpp
#     src/CommonConnectionPool.cpp
#     src/NetServer.cpp
#     src/RankManager.cpp
# )

# 生成可执行程序
add_executable(
    ShortVideoRank
    main.cpp        # 入口文件
    ${SOURCES}      # 自动扫描到的所有 src 目录下的 cpp 文件
)

# 指定连接时需要链接的库
target_link_libraries(
    ShortVideoRank
    pthread         # 多线程支持库
    mysqlclient     # MySQL 客户端官方库
)