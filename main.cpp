// main.cpp
// REPL interactivo del simulador RISC-V (RV32I).
//
//   $ ./rvsim programa.bin
//   > step
//   > regs x5 x14
//   > mem 0x1000 0x1003
//   > exit
#include "simulator.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// --- utilidades de impresion -----------------------------------------------
static std::string hex32(uint32_t v) {
    std::ostringstream os;
    os << "0x" << std::setw(8) << std::setfill('0') << std::hex << v;
    return os.str();
}

// Parsea un entero en decimal o hexadecimal (0x...).
static bool parseU32(const std::string& s, uint32_t& out) {
    try {
        size_t pos = 0;
        unsigned long v = std::stoul(s, &pos, 0);
        if (pos != s.size()) return false;
        out = static_cast<uint32_t>(v);
        return true;
    } catch (...) { return false; }
}

// "x5" / "5" / ABI ("t0","a0","sp"...) -> indice de registro ; -1 si invalido
static int parseReg(const std::string& s) {
    static const std::unordered_map<std::string,int> abi = {
        {"zero",0},{"ra",1},{"sp",2},{"gp",3},{"tp",4},{"t0",5},{"t1",6},{"t2",7},
        {"s0",8},{"fp",8},{"s1",9},{"a0",10},{"a1",11},{"a2",12},{"a3",13},{"a4",14},
        {"a5",15},{"a6",16},{"a7",17},{"s2",18},{"s3",19},{"s4",20},{"s5",21},{"s6",22},
        {"s7",23},{"s8",24},{"s9",25},{"s10",26},{"s11",27},{"t3",28},{"t4",29},{"t5",30},{"t6",31}};
    auto it = abi.find(s);
    if (it != abi.end()) return it->second;
    std::string t = s;
    if (!t.empty() && (t[0] == 'x' || t[0] == 'X')) t = t.substr(1);
    try {
        size_t pos = 0;
        int v = std::stoi(t, &pos, 10);
        if (pos != t.size() || v < 0 || v > 31) return -1;
        return v;
    } catch (...) { return -1; }
}

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    std::string w;
    while (is >> w) out.push_back(w);
    return out;
}

static void printRegs(const Simulator& sim, const std::vector<std::string>& which) {
    if (which.empty()) {
        std::cout << "pc  = " << hex32(sim.pc) << "\n";
        for (int i = 0; i < 32; ++i) {
            std::cout << std::left << std::setw(4) << ("x" + std::to_string(i))
                      << "= " << hex32(sim.rget(i));
            std::cout << ((i % 4 == 3) ? "\n" : "    ");
        }
    } else {
        for (const auto& w : which) {
            int r = parseReg(w);
            if (r < 0) { std::cout << "Registro invalido: " << w << "\n"; continue; }
            std::cout << "x" << r << " = " << hex32(sim.rget(r)) << "\n";
        }
    }
}

static void printMem(const Simulator& sim, uint32_t lo, uint32_t hi) {
    if (hi < lo) std::swap(lo, hi);
    std::cout << "Memoria (" << hex32(lo) << "-" << hex32(hi) << "):";
    for (uint32_t a = lo; ; ++a) {
        std::cout << " " << std::setw(2) << std::setfill('0')
                  << std::hex << static_cast<int>(sim.mem.load8(a));
        if (a == hi) break;
    }
    std::cout << std::dec << "\n";
}

// Ejecuta un paso e informa al usuario.
static bool doStep(Simulator& sim, bool verbose) {
    if (sim.halted) { std::cout << "El programa ya termino.\n"; return false; }
    uint32_t pc   = sim.pc;
    uint32_t inst = sim.mem.load32(pc);
    StepResult r  = sim.step();

    if (verbose) {
        std::cout << "[" << hex32(pc) << "] " << hex32(inst)
                  << "   " << disassemble(inst, pc) << "\n";
    }
    if (r.status == StepStatus::Invalid) {
        std::cout << "Error de ejecucion en " << hex32(pc) << ": " << r.message << "\n";
        sim.halted = true;
        return false;
    }
    if (r.status == StepStatus::Halted) {
        std::cout << "Ejecucion detenida (ebreak).\n";
        return false;
    }
    if (r.status == StepStatus::ExitCall) {
        std::cout << "\nProgram exited with code " << sim.exitCode << ".\n";
        return false;
    }
    return true;
}

static void help() {
    std::cout <<
    "Comandos disponibles:\n"
    "  step [n]            Ejecuta una (o n) instruccion(es)\n"
    "  run [max]           Ejecuta hasta detenerse (limite opcional, def. 1e7)\n"
    "  pc                  Muestra el contador de programa\n"
    "  regs [xN ...]       Muestra todos los registros o los indicados\n"
    "  mem LO HI           Vuelca memoria del rango [LO, HI] (dec o 0x..)\n"
    "  dis [ADDR]          Desensambla la instruccion en ADDR (def. PC)\n"
    "  set xN VAL          Asigna VAL al registro xN\n"
    "  setpc ADDR          Fija el PC\n"
    "  load ARCHIVO        Carga otro binario crudo en 0x0 y resetea\n"
    "  reset               Reinicia el estado (mantiene el programa)\n"
    "  help                Muestra esta ayuda\n"
    "  exit | quit         Sale del simulador\n";
}

static bool loadFile(Simulator& sim, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::cout << "No se pudo abrir el archivo: " << path << "\n"; return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),
                               std::istreambuf_iterator<char>());
    sim.reset();
    sim.load(data, 0);
    std::cout << "\"" << path << "\" cargado a memoria. ("
              << data.size() << " bytes)\n";
    return true;
}

int main(int argc, char** argv) {
    Simulator sim;
    sim.reset();

    if (argc >= 2) {
        if (!loadFile(sim, argv[1])) return 1;
    } else {
        std::cout << "Simulador RISC-V (RV32I). Usa 'load ARCHIVO' o 'help'.\n";
    }

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        auto tok = split(line);
        if (tok.empty()) continue;
        const std::string& cmd = tok[0];

        if (cmd == "exit" || cmd == "quit") {
            std::cout << "See you next time...\n";
            std::cout << "Program exited with code " << sim.exitCode << ".\n";
            break;
        } else if (cmd == "help") {
            help();
        } else if (cmd == "pc") {
            std::cout << "pc = " << hex32(sim.pc) << "\n";
        } else if (cmd == "regs") {
            printRegs(sim, std::vector<std::string>(tok.begin() + 1, tok.end()));
        } else if (cmd == "step") {
            uint32_t n = 1;
            if (tok.size() >= 2 && !parseU32(tok[1], n)) { std::cout << "n invalido\n"; continue; }
            for (uint32_t i = 0; i < n; ++i)
                if (!doStep(sim, true)) break;
        } else if (cmd == "run") {
            uint32_t maxI = 10000000;
            if (tok.size() >= 2) parseU32(tok[1], maxI);
            uint32_t i = 0;
            for (; i < maxI; ++i)
                if (!doStep(sim, false)) break;
            if (i == maxI) std::cout << "Limite de instrucciones alcanzado (" << maxI << ").\n";
        } else if (cmd == "mem") {
            uint32_t lo, hi;
            if (tok.size() < 3 || !parseU32(tok[1], lo) || !parseU32(tok[2], hi)) {
                std::cout << "Uso: mem LO HI\n"; continue;
            }
            printMem(sim, lo, hi);
        } else if (cmd == "dis") {
            uint32_t addr = sim.pc;
            if (tok.size() >= 2 && !parseU32(tok[1], addr)) { std::cout << "ADDR invalida\n"; continue; }
            uint32_t inst = sim.mem.load32(addr);
            std::cout << "[" << hex32(addr) << "] " << hex32(inst)
                      << "   " << disassemble(inst, addr) << "\n";
        } else if (cmd == "set") {
            int r; uint32_t v;
            if (tok.size() < 3 || (r = parseReg(tok[1])) < 0 || !parseU32(tok[2], v)) {
                std::cout << "Uso: set xN VAL\n"; continue;
            }
            sim.rset(r, v);
            std::cout << "x" << r << " = " << hex32(sim.rget(r)) << "\n";
        } else if (cmd == "setpc") {
            uint32_t v;
            if (tok.size() < 2 || !parseU32(tok[1], v)) { std::cout << "Uso: setpc ADDR\n"; continue; }
            sim.pc = v;
            std::cout << "pc = " << hex32(sim.pc) << "\n";
        } else if (cmd == "load") {
            if (tok.size() < 2) { std::cout << "Uso: load ARCHIVO\n"; continue; }
            loadFile(sim, tok[1]);
        } else if (cmd == "reset") {
            // mantiene el binario: lo mas simple es pedir recargar; aqui solo
            // reiniciamos PC/registros sin tocar memoria.
            sim.pc = 0; sim.regs.fill(0); sim.halted = false; sim.exitCode = 0;
            std::cout << "Estado reiniciado (PC=0, registros=0).\n";
        } else {
            std::cout << "Comando desconocido: " << cmd << " (usa 'help')\n";
        }
    }
    return 0;
}
