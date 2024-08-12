#include <iostream>
#include <queue>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

// 假设已经定义了struct ins_ref_t和相关的app_pc类型等
typedef struct _ins_ref_t {
    app_pc pc;
    uint64_t num;
    int opcode;
    int exe_time;
} ins_ref_t;

class Processor {
public:
    std::queue<ins_ref_t> execution_buffer;
    std::vector<ins_ref_t> write_ready;
    int rx_pipe_fd;
    int tx_pipe_fd;

    Processor(int rx_fd, int tx_fd) : rx_pipe_fd(rx_fd), tx_pipe_fd(tx_fd) {}

    void run() {
        bool done = false;
        while (!done) {
            // 每个周期读取四条指令
            for (int i = 0; i < 4; ++i) {
                ins_ref_t ins;
                if (read(rx_pipe_fd, &ins, sizeof(ins_ref_t)) == sizeof(ins_ref_t)) {
                    execution_buffer.push(ins);
                } else {
                    done = true;
                    break;
                }
            }

            // 更新执行缓存区中的指令
            int size = execution_buffer.size();
            for (int i = 0; i < size; ++i) {
                ins_ref_t ins = execution_buffer.front();
                execution_buffer.pop();
                ins.exe_time--;
                if (ins.exe_time > 0) {
                    execution_buffer.push(ins);
                } else {
                    write_ready.push_back(ins);
                }
            }

            // 发射四条指令
            send_instructions();
        }

        // 处理所有剩余的指令
        while (!execution_buffer.empty() || !write_ready.empty()) {
            send_instructions();
        }
    }

private:
    void send_instructions() {
        static uint64_t expected_num = 1;
        int sent_count = 0;

        // 从write ready区域发送指令
        for (int i = 0; i < 4; ++i) {
            auto it = std::find_if(write_ready.begin(), write_ready.end(), [&](const ins_ref_t& ins) {
                return ins.num == expected_num;
            });

            if (it != write_ready.end()) {
                write(tx_pipe_fd, &(*it), sizeof(ins_ref_t));
                write_ready.erase(it);
                expected_num++;
            } else {
                // 发送气泡
                ins_ref_t bubble = {0, expected_num++, -1, 0};
                write(tx_pipe_fd, &bubble, sizeof(ins_ref_t));
            }
            sent_count++;
        }
    }
};

int main() {
    int rx_fd = open("rx_pipe", O_RDONLY);
    int tx_fd = open("tx_pipe", O_WRONLY);

    Processor proc(rx_fd, tx_fd);
    proc.run();

    close(rx_fd);
    close(tx_fd);

    return 0;
}






















#include <iostream>
#include <queue>
#include <set>
#include <unistd.h>
#include <fcntl.h>

// 假设已经定义了struct ins_ref_t和相关的app_pc类型等
typedef struct _ins_ref_t {
    app_pc pc;
    uint64_t num;
    int opcode;
    int exe_time;

    // 为了在set中使用ins_ref_t，需要定义比较运算符
    bool operator<(const ins_ref_t& other) const {
        return num < other.num;
    }
} ins_ref_t;

class Processor {
public:
    std::queue<ins_ref_t> execution_buffer;
    std::set<ins_ref_t> write_ready;
    int rx_pipe_fd;
    int tx_pipe_fd;

    Processor(int rx_fd, int tx_fd) : rx_pipe_fd(rx_fd), tx_pipe_fd(tx_fd) {}

    void run() {
        bool done = false;
        while (!done) {
            // 每个周期读取四条指令
            for (int i = 0; i < 4; ++i) {
                ins_ref_t ins;
                if (read(rx_pipe_fd, &ins, sizeof(ins_ref_t)) == sizeof(ins_ref_t)) {
                    execution_buffer.push(ins);
                } else {
                    done = true;
                    break;
                }
            }

            // 更新执行缓存区中的指令
            int size = execution_buffer.size();
            for (int i = 0; i < size; ++i) {
                ins_ref_t ins = execution_buffer.front();
                execution_buffer.pop();
                ins.exe_time--;
                if (ins.exe_time > 0) {
                    execution_buffer.push(ins);
                } else {
                    write_ready.insert(ins);
                }
            }

            // 发射四条指令
            send_instructions();
        }

        // 处理所有剩余的指令
        while (!execution_buffer.empty() || !write_ready.empty()) {
            send_instructions();
        }
    }

private:
    void send_instructions() {
        static uint64_t expected_num = 1;

        // 从write ready区域发送指令
        for (int i = 0; i < 4; ++i) {
            if (!write_ready.empty() && write_ready.begin()->num == expected_num) {
                auto it = write_ready.begin();
                write(tx_pipe_fd, &(*it), sizeof(ins_ref_t));
                write_ready.erase(it);
                expected_num++;
            } else {
                // 发送气泡
                ins_ref_t bubble = {0, expected_num++, -1, 0};
                write(tx_pipe_fd, &bubble, sizeof(ins_ref_t));
            }
        }
    }
};

int main() {
    int rx_fd = open("rx_pipe", O_RDONLY);
    int tx_fd = open("tx_pipe", O_WRONLY);

    Processor proc(rx_fd, tx_fd);
    proc.run();

    close(rx_fd);
    close(tx_fd);

    return 0;
}
