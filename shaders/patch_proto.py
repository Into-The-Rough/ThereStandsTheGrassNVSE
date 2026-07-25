#prototype of the runtime bytecode patch the plugin performs with d3dx9
#proves the inserted asm assembles for every vanilla grass VS permutation
import re, glob, os, ctypes, sys

DISASM = os.path.join(os.path.dirname(os.path.abspath(__file__)), "disasm")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "patched")
os.makedirs(OUT, exist_ok=True)

d3dx = ctypes.windll.LoadLibrary("d3dx9_38.dll")

class ID3DXBuffer(ctypes.Structure):
    pass

def assemble(src):
    code = ctypes.c_void_p()
    errs = ctypes.c_void_p()
    hr = d3dx.D3DXAssembleShader(src, len(src), None, None, 0,
                                 ctypes.byref(code), ctypes.byref(errs))
    def buf_bytes(p):
        if not p: return b""
        vtbl = ctypes.cast(p, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        GetPointer = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vtbl[3])
        GetSize = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vtbl[4])
        ptr = GetPointer(p)
        size = GetSize(p)
        return ctypes.string_at(ptr, size)
    if hr != 0:
        msg = buf_bytes(errs)
        return None, msg.decode(errors="replace")
    return buf_bytes(code), None

BLOCK = """    add r10.xy, {P}, -c248
    mul r11.x, r10.x, r10.x
    mad r11.x, r10.y, r10.y, r11.x
    add r11.x, r11.x, c249.z
    rsq r11.y, r11.x
    rcp r11.z, r11.y
    mul r11.w, r11.z, c248.w
    add r11.w, -r11.w, c249.z
    max r11.w, r11.w, c249.w
    mul r11.w, r11.w, r11.w
    mul r11.w, r11.w, c249.x
    add r10.z, {P}.z, -c20[a0.x].z
    max r10.z, r10.z, c249.w
    mul r11.w, r11.w, r10.z
    mul r10.xy, r10, r11.y
    mad {P}.xy, r10, r11.w, {P}
"""

def patch(text):
    lines = text.splitlines()
    color = None
    lastAdd = -1
    posReg = None
    dp4Last = (-1, None)
    for i, l in enumerate(lines):
        m = re.search(r"dcl_color (v\d+)", l)
        if m: color = m.group(1)
        m = re.match(r"\s*add (r\d+)\.xyz, r\d+(\.[xyzw]+)?, c20\[a0\.x\]", l)
        if m:
            lastAdd = i
            posReg = m.group(1)
        m = re.match(r"\s*dp4 \S+, c(?:9|1[012]), (r\d+)", l)
        if m: dp4Last = (i, m.group(1))
    if not color or lastAdd < 0 or dp4Last[1] != posReg or dp4Last[0] < lastAdd:
        return None
    if re.search(r"\br1[01]\b", text):
        return None
    block = BLOCK.format(P=posReg)
    #strip the comment header fxc emits, keep from the version token
    start = next(i for i, l in enumerate(lines) if re.match(r"\s*vs_[23]_", l))
    out = lines[start:lastAdd+1] + block.splitlines() + lines[lastAdd+1:]
    return "\n".join(out) + "\n"

ok = fail = 0
for f in sorted(glob.glob(os.path.join(DISASM, "GRASS2*.asm"))):
    text = open(f).read()
    patched = patch(text)
    name = os.path.basename(f)
    if patched is None:
        print("SKIP(pattern)", name); fail += 1; continue
    code, err = assemble(patched.encode())
    if code is None:
        print("FAIL(asm)", name, err); fail += 1; continue
    open(os.path.join(OUT, name.replace(".asm", "")), "wb").write(code)
    ok += 1
print("assembled ok:", ok, "failed:", fail)
