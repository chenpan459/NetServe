CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE -D_XOPEN_SOURCE=700
LIBS = -luv -lpthread

# 目录
SRC_DIR = .
MODULES_DIR = modules
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

# 源文件
MAIN_SRC = $(SRC_DIR)/main.c
MODULE_SRCS = $(wildcard $(MODULES_DIR)/*.c)
ALL_SRCS = $(MAIN_SRC) $(MODULE_SRCS)

# 目标文件
TARGET = $(BUILD_DIR)/tcp_server_multithreaded
OBJS = $(ALL_SRCS:%.c=$(OBJ_DIR)/%.o)

# 默认目标
all: $(TARGET)

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/$(MODULES_DIR)

# 编译主程序
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LIBS)
	@echo "编译完成: $@"



# 编译源文件
$(OBJ_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -rf $(BUILD_DIR)
	@echo "清理完成"

# 运行程序
run: $(TARGET)
	@echo "运行程序..."
	./$(TARGET)

# 测试
test: $(TARGET)
	@echo "启动服务器..."
	./$(TARGET)

# 调试版本
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

# 发布版本
release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)

# 安装
install: $(TARGET)
	@echo "安装程序..."
	cp $(TARGET) /usr/local/bin/
	@echo "安装完成"

# 卸载
uninstall:
	@echo "卸载程序..."
	rm -f /usr/local/bin/tcp_server_modular
	@echo "卸载完成"

# 帮助信息
help:
	@echo "可用的目标:"
	@echo "  all        - 编译程序"
	@echo "  clean      - 清理编译文件"
	@echo "  run        - 运行程序"
	@echo "  test       - 启动服务器进行测试"
	@echo "  debug      - 编译调试版本"
	@echo "  release    - 编译发布版本"
	@echo "  install    - 安装程序"
	@echo "  uninstall  - 卸载程序"
	@echo "  help       - 显示此帮助信息"

# 依赖关系
$(OBJ_DIR)/main.o: $(MAIN_SRC) $(wildcard $(MODULES_DIR)/*.h)
$(OBJ_DIR)/$(MODULES_DIR)/%.o: $(MODULES_DIR)/%.c $(MODULES_DIR)/%.h

.PHONY: all clean run debug release install uninstall help
