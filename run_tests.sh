#!/usr/bin/env bash
# run_tests.sh - Compila, ensambla los demos y corre los 4 ejemplos.
set -e
cd "$(dirname "$0")"

make >/dev/null
echo "== Ensamblando demos =="
for s in demos/*.s; do python3 tools/asm.py "$s" "${s%.s}.bin"; done
echo

echo "== demo1: riscvtest (espera x2=0x19, mem[0x64]=19) =="
printf 'run\nregs x2 x9\nmem 0x64 0x67\nexit\n' | ./rvsim demos/demo1_riscvtest.bin
echo

echo "== demo2: hello (espera 'Hola, RISC-V!') =="
printf 'run\nexit\n' | ./rvsim demos/demo2_hello.bin
echo

echo "== demo3: sumloop (espera 'Suma = 55', mem[0x1000]=37) =="
printf 'run\nmem 0x1000 0x1003\nexit\n' | ./rvsim demos/demo3_sumloop.bin
echo

echo "== demo4: endian (espera t2=0x12345678, t5=0xffffff80, t6=0x80) =="
printf 'run\nregs t2 t5 t6\nmem 0x2000 0x2003\nexit\n' | ./rvsim demos/demo4_endian.bin
