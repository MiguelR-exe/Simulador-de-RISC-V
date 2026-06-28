// simulator.cpp
#include "simulator.hpp"
#include <iostream>
#include <sstream>

// ===========================================================================
//  Memoria (little-endian, dispersa por paginas)
// ===========================================================================
uint8_t Memory::load8(uint32_t addr) const {
    auto it = pages.find(addr / PAGE_SIZE);
    if (it == pages.end()) return 0;            // memoria no escrita == 0
    return it->second[addr % PAGE_SIZE];
}

void Memory::store8(uint32_t addr, uint8_t val) {
    // operator[] crea la pagina (inicializada a 0) si no existe.
    pages[addr / PAGE_SIZE][addr % PAGE_SIZE] = val;
}

// Los accesos de 16/32 bits se componen byte a byte respetando little-endian.
uint16_t Memory::load16(uint32_t addr) const {
    return static_cast<uint16_t>(load8(addr) |
                                 (load8(addr + 1) << 8));
}

uint32_t Memory::load32(uint32_t addr) const {
    return static_cast<uint32_t>(load8(addr)) |
           (static_cast<uint32_t>(load8(addr + 1)) << 8) |
           (static_cast<uint32_t>(load8(addr + 2)) << 16) |
           (static_cast<uint32_t>(load8(addr + 3)) << 24);
}

void Memory::store16(uint32_t addr, uint16_t val) {
    store8(addr,     static_cast<uint8_t>(val & 0xFF));
    store8(addr + 1, static_cast<uint8_t>((val >> 8) & 0xFF));
}

void Memory::store32(uint32_t addr, uint32_t val) {
    store8(addr,     static_cast<uint8_t>(val & 0xFF));
    store8(addr + 1, static_cast<uint8_t>((val >> 8) & 0xFF));
    store8(addr + 2, static_cast<uint8_t>((val >> 16) & 0xFF));
    store8(addr + 3, static_cast<uint8_t>((val >> 24) & 0xFF));
}

void Memory::loadProgram(const std::vector<uint8_t>& data, uint32_t base) {
    for (size_t i = 0; i < data.size(); ++i)
        store8(base + static_cast<uint32_t>(i), data[i]);
}

void Memory::clear() { pages.clear(); }

// ===========================================================================
//  Decodificacion de inmediatos
// ===========================================================================
// Cada formato de RISC-V empaqueta el inmediato de forma distinta dentro de
// los 32 bits de la instruccion. Estas funciones lo reconstruyen y extienden
// el signo cuando corresponde.
static inline int32_t imm_I(uint32_t inst) {
    return static_cast<int32_t>(inst) >> 20;          // inst[31:20], signo
}
static inline int32_t imm_S(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0xFE000000) >> 20) | // imm[11:5]
           ((inst >> 7) & 0x1F);                              // imm[4:0]
}
static inline int32_t imm_B(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0x80000000) >> 19) | // imm[12]+signo
           ((inst & 0x80) << 4) |                            // imm[11]
           ((inst >> 20) & 0x7E0) |                          // imm[10:5]
           ((inst >> 7) & 0x1E);                             // imm[4:1]
}
static inline int32_t imm_U(uint32_t inst) {
    return static_cast<int32_t>(inst & 0xFFFFF000);          // imm[31:12]<<12
}
static inline int32_t imm_J(uint32_t inst) {
    return (static_cast<int32_t>(inst & 0x80000000) >> 11) | // imm[20]+signo
           (inst & 0xFF000) |                                // imm[19:12]
           ((inst >> 9) & 0x800) |                           // imm[11]
           ((inst >> 20) & 0x7FE);                           // imm[10:1]
}

// ===========================================================================
//  Simulador
// ===========================================================================
void Simulator::reset() {
    pc = 0;
    regs.fill(0);
    mem.clear();
    halted = false;
    exitCode = 0;
}

void Simulator::load(const std::vector<uint8_t>& data, uint32_t base) {
    mem.loadProgram(data, base);
}

StepResult Simulator::step() {
    if (halted) return {StepStatus::Halted, ""};

    const uint32_t inst = mem.load32(pc);

    const uint32_t opcode = inst & 0x7F;
    const int      rd     = (inst >> 7)  & 0x1F;
    const int      funct3 = (inst >> 12) & 0x7;
    const int      rs1    = (inst >> 15) & 0x1F;
    const int      rs2    = (inst >> 20) & 0x1F;
    const uint32_t funct7 = (inst >> 25) & 0x7F;
    const uint32_t shamt  = (inst >> 20) & 0x1F;  // shift inmediato (RV32)

    uint32_t next_pc = pc + 4;   // por defecto avanza a la siguiente

    switch (opcode) {

    // ---- LOAD (lb, lh, lw, lbu, lhu) ----------------------------------
    case 0x03: {
        const uint32_t addr = rget(rs1) + static_cast<uint32_t>(imm_I(inst));
        switch (funct3) {
        case 0: rset(rd, static_cast<int32_t>(static_cast<int8_t>(mem.load8(addr))));   break; // lb
        case 1: rset(rd, static_cast<int32_t>(static_cast<int16_t>(mem.load16(addr)))); break; // lh
        case 2: rset(rd, mem.load32(addr));                                             break; // lw
        case 4: rset(rd, mem.load8(addr));                                              break; // lbu
        case 5: rset(rd, mem.load16(addr));                                             break; // lhu
        default: return {StepStatus::Invalid, "funct3 invalido en LOAD"};
        }
        break;
    }

    // ---- OP-IMM (addi, slti, ... slli, srli, srai) --------------------
    case 0x13: {
        const uint32_t a = rget(rs1);
        const int32_t  imm = imm_I(inst);
        switch (funct3) {
        case 0: rset(rd, a + imm); break;                                       // addi
        case 2: rset(rd, (static_cast<int32_t>(a) < imm) ? 1 : 0); break;       // slti
        case 3: rset(rd, (a < static_cast<uint32_t>(imm)) ? 1 : 0); break;      // sltiu
        case 4: rset(rd, a ^ static_cast<uint32_t>(imm)); break;                // xori
        case 6: rset(rd, a | static_cast<uint32_t>(imm)); break;                // ori
        case 7: rset(rd, a & static_cast<uint32_t>(imm)); break;                // andi
        case 1: rset(rd, a << shamt); break;                                    // slli
        case 5:
            if (funct7 == 0x20)                                                 // srai
                rset(rd, static_cast<uint32_t>(static_cast<int32_t>(a) >> shamt));
            else                                                                // srli
                rset(rd, a >> shamt);
            break;
        default: return {StepStatus::Invalid, "funct3 invalido en OP-IMM"};
        }
        break;
    }

    // ---- AUIPC --------------------------------------------------------
    case 0x17:
        rset(rd, pc + static_cast<uint32_t>(imm_U(inst)));
        break;

    // ---- LUI ----------------------------------------------------------
    case 0x37:
        rset(rd, static_cast<uint32_t>(imm_U(inst)));
        break;

    // ---- STORE (sb, sh, sw) -------------------------------------------
    case 0x23: {
        const uint32_t addr = rget(rs1) + static_cast<uint32_t>(imm_S(inst));
        const uint32_t v = rget(rs2);
        switch (funct3) {
        case 0: mem.store8(addr,  static_cast<uint8_t>(v));  break; // sb
        case 1: mem.store16(addr, static_cast<uint16_t>(v)); break; // sh
        case 2: mem.store32(addr, v);                        break; // sw
        default: return {StepStatus::Invalid, "funct3 invalido en STORE"};
        }
        break;
    }

    // ---- OP (add, sub, sll, slt, ...) ---------------------------------
    case 0x33: {
        const uint32_t a = rget(rs1);
        const uint32_t b = rget(rs2);
        const uint32_t sh = b & 0x1F;
        switch (funct3) {
        case 0: rset(rd, (funct7 == 0x20) ? (a - b) : (a + b)); break;          // sub / add
        case 1: rset(rd, a << sh); break;                                       // sll
        case 2: rset(rd, (static_cast<int32_t>(a) < static_cast<int32_t>(b)) ? 1 : 0); break; // slt
        case 3: rset(rd, (a < b) ? 1 : 0); break;                               // sltu
        case 4: rset(rd, a ^ b); break;                                         // xor
        case 5:
            if (funct7 == 0x20)                                                 // sra
                rset(rd, static_cast<uint32_t>(static_cast<int32_t>(a) >> sh));
            else                                                                // srl
                rset(rd, a >> sh);
            break;
        case 6: rset(rd, a | b); break;                                         // or
        case 7: rset(rd, a & b); break;                                         // and
        default: return {StepStatus::Invalid, "funct3 invalido en OP"};
        }
        break;
    }

    // ---- BRANCH (beq, bne, blt, bge, bltu, bgeu) ----------------------
    case 0x63: {
        const uint32_t a = rget(rs1);
        const uint32_t b = rget(rs2);
        bool take = false;
        switch (funct3) {
        case 0: take = (a == b); break;                                                  // beq
        case 1: take = (a != b); break;                                                  // bne
        case 4: take = (static_cast<int32_t>(a) <  static_cast<int32_t>(b)); break;       // blt
        case 5: take = (static_cast<int32_t>(a) >= static_cast<int32_t>(b)); break;       // bge
        case 6: take = (a <  b); break;                                                   // bltu
        case 7: take = (a >= b); break;                                                   // bgeu
        default: return {StepStatus::Invalid, "funct3 invalido en BRANCH"};
        }
        if (take) next_pc = pc + static_cast<uint32_t>(imm_B(inst));
        break;
    }

    // ---- JALR ---------------------------------------------------------
    case 0x67: {
        const uint32_t target =
            (rget(rs1) + static_cast<uint32_t>(imm_I(inst))) & ~1u;
        rset(rd, pc + 4);
        next_pc = target;
        break;
    }

    // ---- JAL ----------------------------------------------------------
    case 0x6F:
        rset(rd, pc + 4);
        next_pc = pc + static_cast<uint32_t>(imm_J(inst));
        break;

    // ---- SYSTEM (ecall / ebreak) --------------------------------------
    case 0x73: {
        const uint32_t imm = (inst >> 20) & 0xFFF;
        if (funct3 == 0 && imm == 0) {            // ecall
            StepResult r = handleEcall();
            if (r.status == StepStatus::Ok) { pc = next_pc; }
            else if (r.status == StepStatus::ExitCall) { halted = true; pc = next_pc; }
            return r;
        }
        if (funct3 == 0 && imm == 1) {            // ebreak -> detener
            halted = true;
            return {StepStatus::Halted, ""};
        }
        return {StepStatus::Invalid, "instruccion SYSTEM no soportada"};
    }

    default: {
        std::ostringstream os;
        os << "opcode no soportado 0x" << std::hex << opcode;
        return {StepStatus::Invalid, os.str()};
    }
    }

    pc = next_pc;
    regs[0] = 0;   // x0 siempre 0 (seguro extra)
    return {StepStatus::Ok, ""};
}

// ---------------------------------------------------------------------------
//  Environment calls (subconjunto estilo SPIM / CPUlator)
//  Convencion RISC-V: a7 (x17) = numero de servicio, a0/a1 = argumentos,
//  a0 = valor de retorno.
// ---------------------------------------------------------------------------
StepResult Simulator::handleEcall() {
    const uint32_t service = rget(17);   // a7
    switch (service) {
    case 1:  // print_int
        std::cout << static_cast<int32_t>(rget(10));
        std::cout.flush();
        return {StepStatus::Ok, ""};

    case 4: { // print_string (a0 = direccion de cadena terminada en \0)
        uint32_t addr = rget(10);
        char c;
        while ((c = static_cast<char>(mem.load8(addr))) != '\0') {
            std::cout << c;
            ++addr;
        }
        std::cout.flush();
        return {StepStatus::Ok, ""};
    }

    case 5: { // read_int -> a0
        int32_t v = 0;
        std::cin >> v;
        rset(10, static_cast<uint32_t>(v));
        return {StepStatus::Ok, ""};
    }

    case 8: { // read_string (a0 = buffer, a1 = longitud maxima)
        uint32_t buf = rget(10);
        uint32_t maxlen = rget(11);
        std::string line;
        std::getline(std::cin, line);
        uint32_t n = 0;
        for (; n + 1 < maxlen && n < line.size(); ++n)
            mem.store8(buf + n, static_cast<uint8_t>(line[n]));
        if (maxlen > 0) mem.store8(buf + n, 0);
        return {StepStatus::Ok, ""};
    }

    case 11: // print_char
        std::cout << static_cast<char>(rget(10) & 0xFF);
        std::cout.flush();
        return {StepStatus::Ok, ""};

    case 12: { // read_char -> a0
        int c = std::cin.get();
        rset(10, static_cast<uint32_t>(c));
        return {StepStatus::Ok, ""};
    }

    case 10: // exit
        exitCode = 0;
        return {StepStatus::ExitCall, ""};

    case 17: // exit2 (a0 = codigo de salida)
        exitCode = static_cast<int>(rget(10));
        return {StepStatus::ExitCall, ""};

    default: {
        std::ostringstream os;
        os << "ecall no soportada (a7 = " << service << ")";
        return {StepStatus::Invalid, os.str()};
    }
    }
}

// ===========================================================================
//  Desensamblador
// ===========================================================================
static std::string hx(int32_t v) {
    std::ostringstream os;
    if (v < 0) os << "-0x" << std::hex << (-v);
    else       os << "0x"  << std::hex << v;
    return os.str();
}
static std::string reg(int i) { return "x" + std::to_string(i); }

std::string disassemble(uint32_t inst, uint32_t pc) {
    const uint32_t opcode = inst & 0x7F;
    const int rd     = (inst >> 7)  & 0x1F;
    const int funct3 = (inst >> 12) & 0x7;
    const int rs1    = (inst >> 15) & 0x1F;
    const int rs2    = (inst >> 20) & 0x1F;
    const uint32_t funct7 = (inst >> 25) & 0x7F;
    const uint32_t shamt  = (inst >> 20) & 0x1F;
    std::ostringstream os;

    auto rrr = [&](const char* m) { os << m << " " << reg(rd) << ", " << reg(rs1) << ", " << reg(rs2); };
    auto rri = [&](const char* m, int32_t imm) { os << m << " " << reg(rd) << ", " << reg(rs1) << ", " << hx(imm); };

    switch (opcode) {
    case 0x03: {
        const char* m[8] = {"lb","lh","lw","?","lbu","lhu","?","?"};
        os << m[funct3] << " " << reg(rd) << ", " << hx(imm_I(inst)) << "(" << reg(rs1) << ")";
        break;
    }
    case 0x13:
        switch (funct3) {
        case 0: rri("addi", imm_I(inst)); break;
        case 2: rri("slti", imm_I(inst)); break;
        case 3: rri("sltiu", imm_I(inst)); break;
        case 4: rri("xori", imm_I(inst)); break;
        case 6: rri("ori", imm_I(inst)); break;
        case 7: rri("andi", imm_I(inst)); break;
        case 1: os << "slli " << reg(rd) << ", " << reg(rs1) << ", " << shamt; break;
        case 5: os << (funct7 == 0x20 ? "srai " : "srli ") << reg(rd) << ", " << reg(rs1) << ", " << shamt; break;
        }
        break;
    case 0x17: os << "auipc " << reg(rd) << ", " << hx(imm_U(inst) >> 12); break;
    case 0x37: os << "lui "   << reg(rd) << ", " << hx(imm_U(inst) >> 12); break;
    case 0x23: {
        const char* m[3] = {"sb","sh","sw"};
        const char* mm = (funct3 < 3) ? m[funct3] : "?";
        os << mm << " " << reg(rs2) << ", " << hx(imm_S(inst)) << "(" << reg(rs1) << ")";
        break;
    }
    case 0x33:
        switch (funct3) {
        case 0: rrr(funct7 == 0x20 ? "sub" : "add"); break;
        case 1: rrr("sll"); break;
        case 2: rrr("slt"); break;
        case 3: rrr("sltu"); break;
        case 4: rrr("xor"); break;
        case 5: rrr(funct7 == 0x20 ? "sra" : "srl"); break;
        case 6: rrr("or"); break;
        case 7: rrr("and"); break;
        }
        break;
    case 0x63: {
        const char* m[8] = {"beq","bne","?","?","blt","bge","bltu","bgeu"};
        os << m[funct3] << " " << reg(rs1) << ", " << reg(rs2)
           << ", " << hx(static_cast<int32_t>(pc + imm_B(inst)));
        break;
    }
    case 0x67: os << "jalr " << reg(rd) << ", " << reg(rs1) << ", " << hx(imm_I(inst)); break;
    case 0x6F: os << "jal "  << reg(rd) << ", " << hx(static_cast<int32_t>(pc + imm_J(inst))); break;
    case 0x73: {
        const uint32_t imm = (inst >> 20) & 0xFFF;
        os << (imm == 0 ? "ecall" : "ebreak");
        break;
    }
    default: os << ".word " << hx(static_cast<int32_t>(inst)); break;
    }
    return os.str();
}
