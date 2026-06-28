#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>


class Memory {
    static constexpr uint32_t PAGE_SIZE = 4096;
    std::unordered_map<uint32_t, std::array<uint8_t, PAGE_SIZE>> pages;

public:
    uint8_t  load8(uint32_t addr) const;
    uint16_t load16(uint32_t addr) const;
    uint32_t load32(uint32_t addr) const;

    void store8(uint32_t addr, uint8_t  val);
    void store16(uint32_t addr, uint16_t val);
    void store32(uint32_t addr, uint32_t val);

    void loadProgram(const std::vector<uint8_t>& data, uint32_t base = 0);
    void clear();
};

enum class StepStatus {
    Ok,
    Halted,
    ExitCall,
    Invalid
};

struct StepResult {
    StepStatus status = StepStatus::Ok;
    std::string message;
};

class Simulator {
public:
    uint32_t pc = 0;
    std::array<uint32_t, 32> regs{};
    Memory mem;

    bool halted = false;
    int  exitCode = 0;

    uint32_t rget(int i) const { return i == 0 ? 0u : regs[i]; }
    void     rset(int i, uint32_t v) { if (i != 0) regs[i] = v; }

    void reset();
    void load(const std::vector<uint8_t>& data, uint32_t base = 0);

    StepResult step();

private:
    StepResult handleEcall();
};

std::string disassemble(uint32_t inst, uint32_t pc);
