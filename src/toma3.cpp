#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unistd.h> // for read/write

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
    int dependency[6] = {0};  // 每个操作数依赖的指令序号
};

struct RunningInstruction {
    uint64_t num;       // 指令序号
    int cycles_left;    // 剩余延时周期数
};

int get_delay(ins_ref_t ins);

void dispatch_instructions(int rx_pipe, int tx_pipe) {
    std::vector<ins_ref_t> instruction_cache;     // 缓存列表
    std::queue<ins_ref_t> ready_queue;            // 准备发射队列
    std::unordered_map<int, RunningInstruction> running_instructions; // 正在运行指令
    int reg_dependency[256] = {0};                // 寄存器依赖表，假设最多256个寄存器
    
    ins_ref_t ins;
    while (read(rx_pipe, &ins, sizeof(ins_ref_t)) > 0 || !instruction_cache.empty() || !ready_queue.empty()) {
        // 处理已经结束的指令
        std::vector<uint64_t> finished_instructions;
        for (auto it = running_instructions.begin(); it != running_instructions.end();) {
            it->second.cycles_left--;
            if (it->second.cycles_left == 0) {
                finished_instructions.push_back(it->first);
                it = running_instructions.erase(it);
            } else {
                ++it;
            }
        }

        // 更新缓存中的指令依赖信息
        for (auto& inst : instruction_cache) {
            for (int i = 0; i < inst.num_operands; ++i) {
                if (inst.operands[i].is_source && reg_dependency[inst.operands[i].value.reg] == 0) {
                    inst.dependency[i] = 0;
                }
            }
        }

        // 将满足条件的指令从缓存移动到准备发射队列
        for (auto it = instruction_cache.begin(); it != instruction_cache.end();) {
            bool ready = true;
            for (int i = 0; i < it->num_operands; ++i) {
                if (it->dependency[i] != 0) {
                    ready = false;
                    break;
                }
            }
            if (ready) {
                ready_queue.push(*it);
                it = instruction_cache.erase(it);
            } else {
                ++it;
            }
        }

        // 读取新的指令并处理依赖信息
        if (read(rx_pipe, &ins, sizeof(ins_ref_t)) > 0) {
            for (int i = 0; i < ins.num_operands; ++i) {
                if (ins.operands[i].is_source) {
                    int reg = ins.operands[i].value.reg;
                    if (reg_dependency[reg] != 0) {
                        ins.dependency[i] = reg_dependency[reg];
                    } else {
                        ins.dependency[i] = 0;
                    }
                }
            }
            instruction_cache.push_back(ins);
        }

        // 发射指令并更新状态
        int dispatched = 0;
        while (dispatched < 4 && !ready_queue.empty()) {
            ins_ref_t to_dispatch = ready_queue.front();
            ready_queue.pop();

            // 更新目标操作数的寄存器依赖
            for (int i = 0; i < to_dispatch.num_operands; ++i) {
                if (to_dispatch.operands[i].is_dest) {
                    reg_dependency[to_dispatch.operands[i].value.reg] = to_dispatch.num;
                }
            }

            // 将发射的指令加入正在运行指令列表
            RunningInstruction running = {to_dispatch.num, get_delay(to_dispatch)};
            running_instructions[to_dispatch.num] = running;

            // 发送指令到tx_pipe
            write(tx_pipe, &to_dispatch, sizeof(ins_ref_t));
            dispatched++;
        }

        // 如果未发射够4条指令，填充气泡
        while (dispatched < 4) {
            ins_ref_t bubble = {};
            write(tx_pipe, &bubble, sizeof(ins_ref_t));
            dispatched++;
        }
    }
}
