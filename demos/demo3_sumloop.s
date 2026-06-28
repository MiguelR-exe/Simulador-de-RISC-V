# demo3_sumloop.s
# Suma 1 + 2 + ... + 10 = 55 con un bucle (branches),
# guarda el resultado en 0x1000 e imprime "Suma = 55".
        li    t0, 0            # acumulador = 0
        li    t1, 1            # i = 1
        li    t2, 11           # limite (i < 11)
loop:
        bge   t1, t2, done     # if i >= 11 -> fin
        add   t0, t0, t1       # acc += i
        addi  t1, t1, 1        # i++
        jal   x0, loop         # repetir
done:
        li    t3, 0x1000
        sw    t0, 0(t3)        # mem[0x1000] = 55

        la    a0, label        # imprime "Suma = "
        li    a7, 4
        ecall
        mv    a0, t0           # imprime el numero (55)
        li    a7, 1
        ecall
        li    a0, 10           # '\n'
        li    a7, 11
        ecall
        li    a7, 10           # exit
        ecall
label:
        .asciz "Suma = "
