# Simulador de RISC-V (RV32I) — Tarea 4 / HW#6

Simulador del ISA base RISC-V de 32 bits. Carga un binario crudo ya compilado,
mantiene el estado arquitectural (PC, 32 registros y memoria) y ejecuta el
código máquina **paso por paso**, permitiendo inspeccionar registros y memoria.

## Lenguaje y estructura

- **Lenguaje:** C++17 (sin dependencias externas).
- `simulator.hpp` / `simulator.cpp` — núcleo: memoria, CPU, decodificación,
  ejecución de las instrucciones, `ecall` y desensamblador.
- `main.cpp` — REPL interactivo (la interfaz por terminal).
- `demos/` — programas de ejemplo en `.s` y sus `.bin` ya ensamblados.

## Compilar y ejecutar

```bash
make                      # produce el ejecutable ./rvsim
./rvsim demos/demo1_riscvtest.bin  # cambiar demo1_riscvtest.bin por el programa q se quiera probar
```

El programa se carga siempre en la dirección `0x00000000`.

## Features

**Obligatorios**

- ISA base RV32I de 32 bits: las 37 instrucciones del anexo A
  (loads/stores, aritmética e inmediatos, shifts, `lui`/`auipc`, branches,
  `jal`/`jalr`).
- Memoria **little-endian**, plana y *byte-addressable* sobre un address space
  de 32 bits. Implementada de forma dispersa por páginas de 4 KiB, así que la
  pila puede crecer en direcciones altas sin reservar 4 GiB.
- Carga de binarios crudos desde archivo.
- **Ejecución paso por paso** controlada por el usuario (`step`).
- Inspección de PC, registros y memoria en cualquier punto.

**Adicionales implementados**

- `ecall` con un subconjunto de las *environment calls* estilo SPIM/CPUlator:
  `print_int` (1), `print_string` (4), `read_int` (5), `read_string` (8),
  `exit` (10), `print_char` (11), `read_char` (12), `exit2` (17).
  La convención es `a7` = número de servicio, `a0`/`a1` = argumentos.
- **Desensamblador** sencillo (`dis`, y se muestra junto a cada `step`).
- Manejo explícito de errores: una instrucción inválida reporta el opcode/funct3
  y la dirección, y detiene la ejecución.

## Comandos del REPL

| Comando | Descripción |
|---|---|
| `step [n]` | Ejecuta una (o `n`) instrucción(es) mostrando su desensamblado |
| `run [max]` | Ejecuta hasta detenerse (límite opcional de instrucciones) |
| `pc` | Muestra el contador de programa |
| `regs [xN/abi ...]` | Muestra todos los registros o los indicados (`x5`, `t0`, `sp`...) |
| `mem LO HI` | Vuelca el rango de memoria `[LO, HI]` (decimal o `0x..`) |
| `dis [ADDR]` | Desensambla la instrucción en `ADDR` (por defecto el PC) |
| `set xN VAL` / `setpc ADDR` | Modifica un registro o el PC |
| `load ARCHIVO` | Carga otro binario crudo y reinicia |
| `reset` | Reinicia PC y registros |
| `help`, `exit`/`quit` | Ayuda / salir |

## Programas de ejemplo (carpeta `demos/`)

1. **demo1_riscvtest** — estilo `riscvtest.s`: ejercita `add, sub, and, or, slt,
   addi, lw, sw, beq, jal`. Resultado: `x2 = 25` y `mem[0x64] = 0x19`.
2. **demo2_hello** — imprime `Hola, RISC-V!` con `ecall` (print_string / print_char).
3. **demo3_sumloop** — bucle con branches que suma 1..10 = 55, lo guarda en
   `0x1000` e imprime `Suma = 55`.
4. **demo4_endian** — accesos byte/half/word y cargas con y sin signo, para
   mostrar el modelo little-endian.

## Notas de diseño

- `x0` está cableado a 0 (las escrituras a `x0` se ignoran).
- Los inmediatos se reconstruyen y extienden el signo según el formato
  (I, S, B, U, J).
- La pila funciona aunque `sp` inicie en 0: al decrementar entra en direcciones
  altas del espacio de 32 bits, que la memoria dispersa maneja sin problema.
- Las cargas con signo (`lb`, `lh`) extienden el signo; las `*u` no.
