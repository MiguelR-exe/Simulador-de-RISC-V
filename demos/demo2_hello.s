# demo2_hello.s
#   a7 = 4  -> print_string (a0 = direccion de la cadena)
#   a7 = 11 -> print_char   (a0 = caracter)
#   a7 = 10 -> exit
        li    a7, 4
        la    a0, msg          # (pseudo) carga direccion de la cadena
        ecall                  # imprime "Hola, RISC-V!"

        li    a7, 11
        li    a0, 10           # '\n'
        ecall                  # salto de linea

        li    a7, 10
        ecall                  # exit
msg:
        .asciz "Hola, RISC-V!"
