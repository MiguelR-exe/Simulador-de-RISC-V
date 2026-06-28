#!/usr/bin/env python3
"""
asm.py - Ensamblador minimo de RV32I para GENERAR binarios de prueba.

NOTA: esto NO forma parte del simulador (el enunciado no pide un ensamblador).
Es solo una herramienta de apoyo para producir archivos .bin de demostracion
a partir de archivos .s legibles, y para verificar el simulador.

Uso:  python3 asm.py entrada.s salida.bin

Soporta el subconjunto RV32I del enunciado, etiquetas, .word, .asciz/.string,
.byte y los pseudo-ops li / la / mv / nop / j / ret.
"""
import sys, re

ABI = {
    "zero":0,"ra":1,"sp":2,"gp":3,"tp":4,"t0":5,"t1":6,"t2":7,
    "s0":8,"fp":8,"s1":9,"a0":10,"a1":11,"a2":12,"a3":13,"a4":14,
    "a5":15,"a6":16,"a7":17,"s2":18,"s3":19,"s4":20,"s5":21,"s6":22,
    "s7":23,"s8":24,"s9":25,"s10":26,"s11":27,"t3":28,"t4":29,"t5":30,"t6":31,
}

def reg(t):
    t = t.strip().lower()
    if t in ABI: return ABI[t]
    if t.startswith("x"): return int(t[1:])
    raise ValueError(f"registro invalido: {t}")

def imm(t, labels=None, pc=None, rel=False):
    t = t.strip()
    if labels is not None and t in labels:
        return labels[t] - pc if rel else labels[t]
    return int(t, 0)

# --- codificadores por formato ---
def R(f7,rs2,rs1,f3,rd,op): return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op
def I(im,rs1,f3,rd,op):     return ((im&0xFFF)<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|op
def S(im,rs2,rs1,f3,op):
    im&=0xFFF
    return (((im>>5)&0x7F)<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|((im&0x1F)<<7)|op
def B(im,rs2,rs1,f3,op):
    im&=0x1FFF
    return (((im>>12)&1)<<31)|(((im>>5)&0x3F)<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(((im>>1)&0xF)<<8)|(((im>>11)&1)<<7)|op
def U(im,rd,op):            return ((im&0xFFFFF)<<12)|(rd<<7)|op
def J(im,rd,op):
    im&=0x1FFFFF
    return (((im>>20)&1)<<31)|(((im>>1)&0x3FF)<<21)|(((im>>11)&1)<<20)|(((im>>12)&0xFF)<<12)|(rd<<7)|op

R_OPS = {  # name -> (funct7, funct3)
    "add":(0,0),"sub":(0x20,0),"sll":(0,1),"slt":(0,2),"sltu":(0,3),
    "xor":(0,4),"srl":(0,5),"sra":(0x20,5),"or":(0,6),"and":(0,7),
}
I_OPS = {  # arith-immediate: name -> funct3
    "addi":0,"slti":2,"sltiu":3,"xori":4,"ori":6,"andi":7,
}
SH_OPS = {"slli":(0,1),"srli":(0,5),"srai":(0x20,5)}
L_OPS  = {"lb":0,"lh":1,"lw":2,"lbu":4,"lhu":5}
S_OPS  = {"sb":0,"sh":1,"sw":2}
B_OPS  = {"beq":0,"bne":1,"blt":4,"bge":5,"bltu":6,"bgeu":7}

MEMRE = re.compile(r"^\s*(-?\w+)\s*\(\s*(\w+)\s*\)\s*$")

def parse_mem(operand):
    m = MEMRE.match(operand)
    if not m: raise ValueError(f"operando de memoria invalido: {operand}")
    return int(m.group(1),0), reg(m.group(2))

def tokenize(line):
    line = line.split("#",1)[0].strip()
    if not line: return None, []
    label = None
    if ":" in line:
        label, line = line.split(":",1)
        label = label.strip()
        line = line.strip()
    if not line: return label, []
    parts = line.split(None,1)
    mnem = parts[0].lower()
    args = [a.strip() for a in parts[1].split(",")] if len(parts)>1 else []
    return label, [mnem]+args

def expand_pseudo(mnem, args):
    """Devuelve lista de instrucciones reales (puede ser >1)."""
    if mnem == "nop": return [["addi","x0","x0","0"]]
    if mnem == "mv":  return [["addi",args[0],args[1],"0"]]
    if mnem == "ret": return [["jalr","x0","ra","0"]]
    if mnem == "j":   return [["jal","x0",args[0]]]
    if mnem == "la":  return [["addi",args[0],"x0",args[1]]]  # direcciones bajas
    if mnem == "li":
        rd, v = args[0], int(args[1],0)
        if -2048 <= v <= 2047:
            return [["addi",rd,"x0",str(v)]]
        # lui + addi (con correccion de signo del addi)
        hi = (v + 0x800) >> 12
        lo = v - (hi<<12)
        return [["lui",rd,str(hi & 0xFFFFF)],["addi",rd,rd,str(lo)]]
    return [[mnem]+args]

def assemble(text):
    # paso 1: expandir pseudos y calcular direcciones / etiquetas
    items = []   # (addr, mnem_args) o ('data', bytes)
    labels = {}
    addr = 0
    for raw in text.splitlines():
        label, tok = tokenize(raw)
        if label: labels[label] = addr
        if not tok: continue
        mnem = tok[0]
        if mnem == ".word":
            for v in tok[1:]:
                items.append((addr, ("data", int(v,0).to_bytes(4,"little"))))
                addr += 4
            continue
        if mnem in (".byte",):
            for v in tok[1:]:
                items.append((addr, ("data", bytes([int(v,0)&0xFF]))))
                addr += 1
            continue
        if mnem in (".asciz",".string",".ascii"):
            s = raw.split(None,1)[1].strip()
            s = bytes(s[1:-1], "utf-8").decode("unicode_escape").encode("latin-1")
            if mnem != ".ascii": s += b"\x00"
            items.append((addr, ("data", s)))
            addr += len(s)
            # alinea a 4 para la siguiente instruccion
            while addr % 4: addr += 1
            continue
        for ins in expand_pseudo(mnem, tok[1:]):
            items.append((addr, ("ins", ins)))
            addr += 4

    # paso 2: codificar
    out = bytearray()
    for a, (kind, payload) in items:
        if kind == "data":
            # rellenar hasta 'a' por si hubo alineacion
            while len(out) < a: out.append(0)
            out += payload
            continue
        while len(out) < a: out.append(0)
        ins = payload
        m = ins[0]; ar = ins[1:]
        if m in R_OPS:
            f7,f3 = R_OPS[m]; w = R(f7,reg(ar[2]),reg(ar[1]),f3,reg(ar[0]),0x33)
        elif m in I_OPS:
            f3 = I_OPS[m]; w = I(imm(ar[2],labels),reg(ar[1]),f3,reg(ar[0]),0x13)
        elif m in SH_OPS:
            f7,f3 = SH_OPS[m]; sh = imm(ar[2])&0x1F
            w = I((f7<<5)|sh, reg(ar[1]), f3, reg(ar[0]), 0x13)
        elif m in L_OPS:
            off,rs1 = parse_mem(ar[1]); w = I(off,rs1,L_OPS[m],reg(ar[0]),0x03)
        elif m in S_OPS:
            off,rs1 = parse_mem(ar[1]); w = S(off,reg(ar[0]),rs1,S_OPS[m],0x23)
        elif m in B_OPS:
            off = imm(ar[2],labels,a,rel=True); w = B(off,reg(ar[1]),reg(ar[0]),B_OPS[m],0x63)
        elif m == "lui":
            w = U(imm(ar[1]),reg(ar[0]),0x37)
        elif m == "auipc":
            w = U(imm(ar[1]),reg(ar[0]),0x17)
        elif m == "jal":
            off = imm(ar[1],labels,a,rel=True); w = J(off,reg(ar[0]),0x6F)
        elif m == "jalr":
            w = I(imm(ar[2]),reg(ar[1]),0,reg(ar[0]),0x67)
        elif m == "ecall":
            w = 0x00000073
        elif m == "ebreak":
            w = 0x00100073
        else:
            raise ValueError(f"instruccion no soportada por el ensamblador: {m}")
        out += (w & 0xFFFFFFFF).to_bytes(4,"little")
    return bytes(out)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Uso: python3 asm.py entrada.s salida.bin"); sys.exit(1)
    with open(sys.argv[1]) as f: src = f.read()
    binr = assemble(src)
    with open(sys.argv[2],"wb") as f: f.write(binr)
    print(f"{sys.argv[2]}: {len(binr)} bytes")
