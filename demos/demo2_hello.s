# demo2_hello.s
# Imprime un texto usando environment calls estilo SPIM/CPUlator y termina.
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

# la (load address) no es real en este mini-ensamblador, asi que cargamos
# la direccion conocida de 'msg' de forma explicita mas abajo.
# Para mantenerlo simple, la cadena se coloca con una etiqueta y el
# ensamblador resuelve 'la' como li con la direccion absoluta.
msg:
        .asciz "Hola, RISC-V!"
