# demo1_riscvtest.s
# Version propia (estilo riscvtest.s) que ejercita:
#   add, sub, and, or, slt, addi, lw, sw, beq, jal
# Resultado esperado: x2 = 25 (0x19) y mem[0x64] = 25.
        addi  x2, x0, 5        # x2 = 5
        addi  x3, x0, 12       # x3 = 12
        addi  x7, x3, -9       # x7 = 3
        or    x4, x7, x2       # x4 = 3 | 5  = 7
        and   x5, x3, x4       # x5 = 12 & 7 = 4
        add   x5, x5, x4       # x5 = 4 + 7  = 11
        beq   x5, x7, end      # 11 == 3 ? no -> no salta
        slt   x4, x3, x4       # x4 = (12 < 7) ? 0
        beq   x4, x0, around   # 0 == 0 ? si -> salta
        addi  x5, x0, 0        # (saltada)
around:
        slt   x4, x7, x2       # x4 = (3 < 5) ? 1
        add   x7, x4, x5       # x7 = 1 + 11 = 12
        sub   x7, x7, x2       # x7 = 12 - 5 = 7
        sw    x7, 0x64(x0)     # mem[0x64] = 7
        lw    x2, 0x64(x0)     # x2 = 7
        add   x9, x2, x5       # x9 = 7 + 11 = 18
        jal   x3, end          # salto (x3 = pc+4)
        addi  x2, x0, 1        # (saltada)
end:
        add   x2, x2, x9       # x2 = 7 + 18 = 25
        sw    x2, 0x64(x0)     # mem[0x64] = 25 (0x19)
        ebreak                 # detener
