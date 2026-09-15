import sys
def decode(w):
    # MOVN (sf=1, opc=00, 100101): 0x92800000 base
    if (w & 0xff800000)==0x92800000:
        hw=(w>>21)&3; imm=(w>>5)&0xffff; rd=w&0x1f
        val=~(imm << (hw*16)) & 0xffffffffffffffff
        return f"MOVN x{rd}, #{imm} lsl {hw*16}  => x{rd} = {val if val<2**63 else val-2**64}"
    if (w & 0xff800000)==0xd2800000:
        hw=(w>>21)&3; imm=(w>>5)&0xffff; rd=w&0x1f
        return f"MOVZ x{rd}, #{imm} lsl {hw*16}"
    if (w & 0xff800000)==0xf2800000:
        hw=(w>>21)&3; imm=(w>>5)&0xffff; rd=w&0x1f
        return f"MOVK x{rd}, #{imm} lsl {hw*16}"
    if w==0xd4001001: return "SVC #0x80"
    if w==0xd65f03c0: return "RET"
    if (w & 0xfffffc1f)==0xd4000001: return f"SVC #{w>>5&0xffff}"
    return f"0x{w:08x}"
for line in sys.stdin:
    line=line.strip()
    if not line: continue
    for tok in line.split():
        try: w=int(tok,16)
        except: continue
        print(decode(w))
