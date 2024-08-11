#include <vector>
#include <queue>
#include <map>
#include <iostream>
#include <algorithm>
#include <cstring>

struct operand_t {
    operand_type_t type;
    bool is_source;
    bool is_dest;
    union {
        int reg;
        void *mem_addr;
        int imm_val;
    } value;
};

struct ins_ref_t {
    app_pc pc;
    uint64_t num;
    int opcode;
    int num_operands;
    operand_t operands[4];
};

// 假设存在这样的函数
extern ins_ref_t read_from_rx_pipe();
extern void write_to_tx_pipe(const std::vector<ins_ref_t>& ins);
extern int get_delay(ins_ref_t ins);

struct ins_entry {
    ins_ref_t ins;
    uint64_t dependencies[6] = {0};  // 假设最多有6个依赖
};

struct running_instruction {
    uint64_t ins_num;
    int cycles_left;
};

std::vector<ins_entry> instruction_cache;
std::queue<ins_entry> ready_to_issue;
std::vector<running_instruction> running_instructions;

// 用于跟踪寄存器的最后一次写入指令
std::map<int, uint64_t> register_last_written;

void process_instructions() {
    bool end_of_pipe = false;
    while (!end_of_pipe || !instruction_cache.empty() || !ready_to_issue.empty()) {
        // 每个周期尝试读取四条指令
        for (int i = 0; i < 4 && !end_of_pipe; ++i) {
            ins_ref_t ins = read_from_rx_pipe();
            if (/* 检查是否读到了管道的末尾 */ false) {
                end_of_pipe = true;
                break;
            }

            ins_entry new_entry;
            new_entry.ins = ins;
            // 检查每个源操作数的依赖
            for (int j = 0; j < ins.num_operands; ++j) {
                operand_t& op = ins.operands[j];
                if (op.is_source) {
                    int reg = op.value.reg;  // 假设操作数类型正确处理
                    if (register_last_written.find(reg) != register_last_written.end()) {
                        new_entry.dependencies[j] = register_last_written[reg];
                    }
                }
            }
            instruction_cache.push_back(new_entry);
        }

        // 更新正在运行的指令
        for (auto it = running_instructions.begin(); it != running_instructions.end(); ) {
            if (--(it->cycles_left) == 0) {
                // 写回完成，更新依赖为0
                for (auto& entry : instruction_cache) {
                    for (uint64_t& dep : entry.dependencies) {
                        if (dep == it->ins_num) {
                            dep = 0;
                        }
                    }
                }
                it = running_instructions.erase(it);
            } else {
                ++it;
            }
        }

        // 检查指令缓存，移动到准备发射队列
        for (auto it = instruction_cache.begin(); it != instruction_cache.end(); ) {
            if (std::all_of(std::begin(it->dependencies), std::end(it->dependencies), [](uint64_t dep) { return dep == 0; })) {
                ready_to_issue.push(*it);
                it = instruction_cache.erase(it);
            } else {
                ++it;
            }
        }

        // 发射指令
        std::vector<ins_ref_t> to_issue;
        for (int i = 0; i < 4 && !ready_to_issue.empty(); ++i) {
            ins_entry entry = ready_to_issue.front();
            ready_to_issue.pop();
            for (int j = 0; j < entry.ins.num_operands; ++j) {
                operand_t& op = entry.ins.operands[j];
                if (op.is_dest) {
                    register_last_written[op.value.reg] = entry.ins.num;
                }
            }
            running_instructions.push_back({entry.ins.num, get_delay(entry.ins)});
            to_issue.push_back(entry.ins);
        }

        // 填充气泡
        while (to_issue.size() < 4) {
            ins_ref_t bubble = {};  // 创建一个空指令作为气泡
            to_issue.push_back(bubble);
        }

        write_to_tx_pipe(to_issue);
    }
}
