#include <vector>
#include <queue>
#include <unordered_map>
#include <iostream>

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
std::unordered_map<int, uint64_t> register_last_written;

void process_instructions() {
    bool end_of_pipe = false;
    while (!end_of_pipe || !instruction_cache.empty() || !ready_to_issue.empty()) {
        std::vector<ins_entry> new_entries;
        // 每个周期读取四条指令
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
            new_entries.push_back(new_entry);
        }

        // 更新正在运行的指令，并检查是否有指令结束
        for (auto it = running_instructions.begin(); it != running_instructions.end();) {
            if (--(it->cycles_left) == 0) {
                // 对结束的指令更新依赖项
                uint64_t finished_ins_num = it->ins_num;
                for (auto& entry : instruction_cache) {
                    for (uint64_t& dep : entry.dependencies) {
                        if (dep == finished_ins_num) {
                            dep = 0;
                        }
                    }
                }
                for (auto& entry : new_entries) {
                    for (uint64_t& dep : entry.dependencies) {
                        if (dep == finished_ins_num) {
                            dep = 0;
                        }
                    }
                }
                it = running_instructions.erase(it);
            } else {
                ++it;
            }
        }

        // 新指令加入缓存并检查是否可发射
        for (auto& entry : new_entries) {
            if (std::all_of(std::begin(entry.dependencies), std::end(entry.dependencies), [](uint64_t dep) { return dep == 0; })) {
                ready_to_issue.push(entry);
            } else {
                instruction_cache.push_back(entry);
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




// 更新正在运行的指令，并检查是否有指令结束
for (auto it = running_instructions.begin(); it != running_instructions.end();) {
    if (--(it->cycles_left) == 0) {
        // 对结束的指令更新依赖项
        uint64_t finished_ins_num = it->ins_num;
        for (auto it_cache = instruction_cache.begin(); it_cache != instruction_cache.end();) {
            bool can_issue = true;
            for (uint64_t& dep : it_cache->dependencies) {
                if (dep == finished_ins_num) {
                    dep = 0;  // 依赖项归零
                }
                if (dep != 0) {
                    can_issue = false;  // 检查是否所有依赖项均已归零
                }
            }
            if (can_issue) {
                ready_to_issue.push(*it_cache);  // 可以发射的指令加入准备发射队列
                it_cache = instruction_cache.erase(it_cache);  // 从缓存中移除
            } else {
                ++it_cache;
            }
        }
        it = running_instructions.erase(it);  // 从运行指令列表中移除已完成的指令
    } else {
        ++it;
    }
}

// 新指令加入缓存并检查是否可发射
for (auto& entry : new_entries) {
    bool can_issue = true;
    for (uint64_t dep : entry.dependencies) {
        if (dep != 0) {
            can_issue = false;  // 检查是否所有依赖项均已归零
            break;
        }
    }
    if (can_issue) {
        ready_to_issue.push(entry);  // 可以发射的指令加入准备发射队列
    } else {
        instruction_cache.push_back(entry);  // 不可发射的指令加入缓存
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
    ins_ref_t bubble;
    if (instruction_cache.empty()) {
        bubble = frontend_bubble;  // 前端气泡，指令获取阶段的瓶颈
    } else {
        bubble = backend_bubble;  // 后端气泡，执行阶段的瓶颈
    }
    to_issue.push_back(bubble);
}

write_to_tx_pipe(to_issue);




// 假设最大功能部件编号为 N-1
const int N = 10; // 根据实际最大功能部件编号调整这个值
int total_units[N]; // 存储每个功能部件的总数量
int used_units[N];  // 跟踪每个功能部件的使用数量



// 初始化功能部件总数
total_units[0] = 4;
total_units[1] = 2;
// 确保未明确初始化的部件总数为0或适当的默认值
for (int i = 2; i < N; ++i) {
    total_units[i] = 0;  // 或其他适当的值
}

// 发射指令，检查并更新功能部件使用情况
for (int i = 0; i < 4 && !ready_to_issue.empty(); ++i) {
    ins_entry entry = ready_to_issue.front();
    uint32_t unit_needed = opcode2exe(entry.ins);

    if (unit_needed < N && used_units[unit_needed] < total_units[unit_needed]) {
        ready_to_issue.pop();
        used_units[unit_needed]++;
        running_instructions.push_back({entry.ins.num, get_delay(entry.ins)});
        to_issue.push_back(entry.ins);
    } else {
        break;  // 功能部件不足时停止发射更多指令
    }
}

// 指令完成，释放功能部件
for (auto it = running_instructions.begin(); it != running_instructions.end();) {
    if (--(it->cycles_left) == 0) {
        uint32_t unit_used = opcode2exe(/* 获取该指令信息 */);
        if (unit_used < N) {
            used_units[unit_used]--;
        }
        it = running_instructions.erase(it);
    } else {
        ++it;
    }
}

