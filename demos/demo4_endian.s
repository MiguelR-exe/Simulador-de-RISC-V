# demo4_endian.s
# Demuestra el modelo de memoria little-endian con accesos de
# byte / halfword / word y carga con/sin signo.
        li    t0, 0x2000

        li    t1, 0x12345678
        sw    t1, 0(t0)        # mem[0x2000..0x2003] = 78 56 34 12 (little-endian)

        lw    t2, 0(t0)        # t2 = 0x12345678
        lbu   t3, 0(t0)        # t3 = 0x78 (byte mas bajo)
        lhu   t4, 0(t0)        # t4 = 0x5678 (halfword bajo)

        li    t1, 0xFFFFFF80    # -128 en complemento a 2
        sb    t1, 8(t0)        # mem[0x2008] = 0x80
        lb    t5, 8(t0)        # t5 = 0xFFFFFF80 (extiende signo)
        lbu   t6, 8(t0)        # t6 = 0x00000080 (sin signo)

        ebreak
