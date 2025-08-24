cmake_minimum_required(VERSION 3.10)
project(NetServe C)

# 设置C标准
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE -D_XOPEN_SOURCE=700")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖库
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUV REQUIRED libuv)

# 查找pthread库
find_package(Threads REQUIRED)

# 源文件
set(MAIN_SRC src/main.c)

# 模块源文件
file(GLOB MODULE_SRCS 
    "src/modules/*.c"
    "src/log/*.c"
    "src/http/*.c"
    "src/config/*.c"
    "src/memory/*.c"
    "src/thread/*.c"
    "src/net/*.c"
    "src/json/*.c"
)

# 所有源文件
set(ALL_SRCS ${MAIN_SRC} ${MODULE_SRCS})

# 创建可执行文件
add_executable(tcp_server_multithreaded ${ALL_SRCS})

# 设置包含目录
target_include_directories(tcp_server_multithreaded PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modules
    ${CMAKE_CURRENT_SOURCE_DIR}/src/http
    ${CMAKE_CURRENT_SOURCE_DIR}/src/log
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config
    ${CMAKE_CURRENT_SOURCE_DIR}/src/memory
    ${CMAKE_CURRENT_SOURCE_DIR}/src/thread
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_SOURCE_DIR}/src/json
    ${LIBUV_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(tcp_server_multithreaded
    ${LIBUV_LIBRARIES}
    Threads::Threads
)

# 设置编译选项
target_compile_options(tcp_server_multithreaded PRIVATE
    ${LIBUV_CFLAGS_OTHER}
)

# 设置链接选项
target_link_options(tcp_server_multithreaded PRIVATE
    ${LIBUV_LDFLAGS_OTHER}
)

# 调试版本
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -DDEBUG")

# 发布版本
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -DNDEBUG")

# 安装规则
install(TARGETS tcp_server_multithreaded
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(DIRECTORY config/
    DESTINATION etc/netserve
    FILES_MATCHING PATTERN "*.ini"
)

# 自定义目标：运行程序
add_custom_target(run
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tcp_server_multithreaded
    DEPENDS tcp_server_multithreaded
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "运行程序..."
)

# 自定义目标：测试
add_custom_target(test
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tcp_server_multithreaded
    DEPENDS tcp_server_multithreaded
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "启动服务器进行测试..."
)

# 显示配置信息
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "C编译器: ${CMAKE_C_COMPILER}")
message(STATUS "C标准: ${CMAKE_C_STANDARD}")
message(STATUS "输出目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "libuv版本: ${LIBUV_VERSION}")
message(STATUS "libuv库: ${LIBUV_LIBRARIES}")
message(STATUS "libuv包含目录: ${LIBUV_INCLUDE_DIRS}")
