#include <iostream>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>

// 全局状态
static bool running = true;

// 一个极其简单的处理函数，只打印它收到的内容
void process_line_simply(const std::string& line) {
    if (line == "quit" || line == "exit") {
        running = false;
    }
    std::cout << "SUCCESSFULLY PROCESSED: [" << line << "]" << std::endl;
}

// C风格的回调函数
void line_handler(char* line) {
    if (!line) {
        running = false;
        std::cout << std::endl;
        return;
    }
    if (line[0] != '\0') {
        add_history(line);
        process_line_simply(line);
    }
    free(line);
}

int main() {
    std::cout << "--- Minimal Readline Callback Test ---" << std::endl;

    rl_callback_handler_install("> ", line_handler);

    while (running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);

        // 等待输入
        select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);

        // 读取字符
        rl_callback_read_char();
    }

    rl_callback_handler_remove();
    std::cout << "--- Test Finished ---" << std::endl;
    return 0;
}