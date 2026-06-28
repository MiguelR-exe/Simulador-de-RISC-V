// simulator.hpp
// Simulador del ISA base RISC-V de 32 bits (RV32I).
// Mantiene el estado arquitectural: PC, 32 registros y memoria plana
// byte-addressable de 32 bits en formato little-endian.
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Memoria
// ---------------------------------------------------------------------------
// Memoria plana de 32 bits implementada de forma dispersa (sparse) por paginas
// de 4 KiB. Esto permite un address space completo de 4 GiB sin reservar 4 GiB
// reales: solo se materializan las paginas que el programa realmente toca
// (codigo, datos en 0x1000, pila en direcciones altas, etc.).
//
// Todos los accesos multi-byte usan little-endian, como exige RISC-V: el byte
// menos significativo se guarda en la direccion mas baja.
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

    // Carga un binario crudo en memoria a partir de 'base'.
    void loadProgram(const std::vector<uint8_t>& data, uint32_t base = 0);
    void clear();
};

// ---------------------------------------------------------------------------
// Resultado de ejecutar una instruccion
// ---------------------------------------------------------------------------
enum class StepStatus {
    Ok,          // instruccion ejecutada con normalidad
    Halted,      // ebreak: detener ejecucion
    ExitCall,    // ecall de salida (exit / exit2)
    Invalid      // instruccion invalida / no soportada
};

struct StepResult {
    StepStatus status = StepStatus::Ok;
    std::string message;   // descripcion del error si status == Invalid
};

// ---------------------------------------------------------------------------
// Simulador (CPU)
// ---------------------------------------------------------------------------
class Simulator {
public:
    uint32_t pc = 0;
    std::array<uint32_t, 32> regs{};   // x0..x31 (x0 siempre vale 0)
    Memory mem;

    bool halted = false;
    int  exitCode = 0;

    // Acceso a registros respetando que x0 esta cableado a 0.
    uint32_t rget(int i) const { return i == 0 ? 0u : regs[i]; }
    void     rset(int i, uint32_t v) { if (i != 0) regs[i] = v; }

    void reset();
    void load(const std::vector<uint8_t>& data, uint32_t base = 0);

    // Lee la instruccion apuntada por PC, la decodifica, ejecuta su
    // comportamiento y actualiza el estado. Devuelve el resultado.
    StepResult step();

private:
    // Implementacion de las environment calls estilo SPIM/CPUlator.
    StepResult handleEcall();
};

// ---------------------------------------------------------------------------
// Desensamblador
// ---------------------------------------------------------------------------
// Traduce una instruccion de 32 bits a su forma textual (ej. "addi x5, x0, 5").
// 'pc' se usa para resolver los destinos absolutos de saltos y branches.
std::string disassemble(uint32_t inst, uint32_t pc);
