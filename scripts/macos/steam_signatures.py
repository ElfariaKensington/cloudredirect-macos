#!/usr/bin/env python3
"""Derive build-independent signatures for a Steam steamclient.dylib (arm64).

usage: steam_signatures.py REFERENCE.arm64 TARGET.arm64 [--json out.json]

REFERENCE is a thin arm64 slice of a build whose per-function RVAs are known (kBuilds entry).
The tool masks layout-dependent instruction fields, finds the shortest signature that is unique in
both binaries and reports where the same function sits in TARGET. Use `lipo -thin arm64`.
"""
import json, struct, sys
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

# RVAs in the 1792618285 build (first kBuilds entry).
REF_FUNCS = {
    "getAppInfo": 0x59fd60, "getSection": 0x59c108, "readConfigU64": 0x59fefc,
    "kvFindKey": 0x1096b60, "kvGetUint64": 0x1097d50, "kvGetInt": 0x1097acc,
    "kvSetUint64": 0x1098644, "kvSetInt": 0x1098608, "kvSetString": 0x1098420,
    "serialize": 0xb3e738, "parse": 0xb3e6e8, "send": 0x11450ac, "writer": 0xa7d1b0, "manifest": 0x5b0a98,
}
REF_ENGINE_GLOBAL, REF_CACHE_OFFSET = 0x1876438, 0x1518


class Macho:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        magic, _, _, _, ncmds, _, _, _ = struct.unpack_from("<IiiIIIII", self.data, 0)
        assert magic == 0xFEEDFACF, "not a thin 64-bit Mach-O"
        self.segs = []
        off = 32
        for _ in range(ncmds):
            cmd, size = struct.unpack_from("<II", self.data, off)
            if cmd == 0x19:
                name = self.data[off + 8:off + 24].split(b"\0")[0].decode()
                vmaddr, vmsize, fileoff, filesize, maxp, initp = struct.unpack_from("<QQQQii", self.data, off + 24)
                self.segs.append((name, vmaddr, vmsize, fileoff, filesize, initp))
            off += size
        self.text = [s for s in self.segs if s[5] & 4]          # executable segments

    def rva_to_off(self, rva):
        for _, va, vs, fo, fs, _ in self.segs:
            if va <= rva < va + min(vs, fs):
                return fo + rva - va
        return None

    def read(self, rva, n):
        o = self.rva_to_off(rva)
        return self.data[o:o + n]

    def u32_words(self):
        for _, va, vs, fo, fs, _ in self.text:
            yield va, struct.unpack_from("<%dI" % (fs // 4), self.data, fo)


def masked_word(w):
    """Return (value, mask) for one instruction word; mask bits are the *kept* bits."""
    full = 0xFFFFFFFF
    if w & 0x9F000000 == 0x90000000 or w & 0x9F000000 == 0x10000000:      # ADRP / ADR
        return w & 0x9F00001F, 0x9F00001F
    if w & 0x7C000000 == 0x14000000:                                       # B / BL
        return w & 0xFC000000, 0xFC000000
    if w & 0xFF000010 == 0x54000000:                                       # B.cond
        return w & 0xFF00001F, 0xFF00001F
    if w & 0x7E000000 == 0x34000000:                                       # CBZ / CBNZ
        return w & 0xFF00001F, 0xFF00001F
    if w & 0x7E000000 == 0x36000000:                                       # TBZ / TBNZ
        return w & 0xFFF8001F, 0xFFF8001F
    if w & 0x3B000000 == 0x18000000:                                       # LDR literal
        return w & 0xFF00001F, 0xFF00001F
    return w, full


def signature(words, n, follow_adrp=True, loose=False):
    """Masked bytes for the first n instructions. imm12 after ADRP (same reg) is masked too."""
    out, adrp_regs = [], set()
    for w in words[:n]:
        v, m = masked_word(w)
        if w & 0x9F000000 == 0x90000000:
            adrp_regs.add(w & 31)
        elif loose and (w & 0x3B000000 == 0x39000000):                     # LDR/STR unsigned imm12 (field offset)
            m &= ~(0xFFF << 10)
            v &= m
        elif loose and (w & 0x7E000000 == 0x28000000 or w & 0x7E000000 == 0x2C000000):  # LDP/STP imm7
            m &= ~(0x7F << 15)
            v &= m
        elif follow_adrp:
            # ADD imm (sf=1) / LDR/STR unsigned imm whose base is an ADRP result
            rn = (w >> 5) & 31
            if rn in adrp_regs and (w & 0xFF800000 == 0x91000000 or w & 0x3B000000 == 0x39000000):
                m &= ~(0xFFF << 10)
                v &= m
        out.append(byte_round(v, m))
    return out


def byte_round(v, m):
    """The runtime matches whole bytes (`??`), so a partially masked byte is masked entirely."""
    mm = 0
    for i in range(4):
        if (m >> (8 * i)) & 0xFF == 0xFF:
            mm |= 0xFF << (8 * i)
    return v & mm, mm


def sig_bytes(sig):
    b = []
    for v, m in sig:
        for i in range(4):
            mb = (m >> (8 * i)) & 0xFF
            b.append(None if mb != 0xFF else (v >> (8 * i)) & 0xFF)
    return b


def sig_text(sig):
    return " ".join("??" if x is None else "%02X" % x for x in sig_bytes(sig))


def find_all(macho, sig, limit=3):
    """Instruction-aligned search of masked word signature over executable segments."""
    hits = []
    n = len(sig)
    firstv, firstm = sig[0]
    for va, words in macho.u32_words():
        for i in range(len(words) - n + 1):
            if words[i] & firstm != firstv:
                continue
            ok = True
            for j in range(1, n):
                v, m = sig[j]
                if words[i + j] & m != v:
                    ok = False
                    break
            if ok:
                hits.append(va + 4 * i)
                if len(hits) >= limit:
                    return hits
    return hits


def func_words(macho, rva, n):
    return list(struct.unpack("<%dI" % n, macho.read(rva, 4 * n)))


def shortest_unique(ref, tgt, rva, lo=8, hi=64, loose=False):
    words = func_words(ref, rva, hi)
    best = None
    for n in range(lo, hi + 1):
        sig = signature(words, n, loose=loose)
        r = find_all(ref, sig)
        if r != [rva]:
            continue
        t = find_all(tgt, sig)
        if len(t) == 1:
            return n, sig, t[0], True
        if best is None:
            best = (n, sig, t, False)
    return best


def disasm(macho, rva, n):
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    return [(i.address, i.mnemonic, i.op_str) for i in md.disasm(macho.read(rva, n * 4), rva)]


def emit_header(out, tgt, path):
    """Second generation: signatures taken from TARGET itself (the newest layout), merged with the
    reference ones. The runtime tries every candidate and demands that all unique hits agree."""
    lines = ["// Generated by scripts/macos/steam_signatures.py --emit-header. Do not edit by hand.",
             "// Candidates per function: reference build (strict, loose), then newest build (strict, loose).",
             "// The runtime requires every candidate that matches uniquely to point at the same address.",
             "#pragma once", "namespace MacProfile {", "struct SigSet { const char* name; const char* candidates[4]; };",
             "static const SigSet kSignatures[] = {"]
    for name, levels in out["functions"].items():
        cands = []
        for lv in ("strict", "loose"):
            if "signature" in levels[lv]:
                cands.append(levels[lv]["signature"])
        rva = levels["strict"].get("target_rva") or levels["loose"].get("target_rva")
        if rva:
            for loose in (False, True):
                r = shortest_unique(tgt, tgt, rva, loose=loose)
                if r and r[3] is True:
                    cands.append(sig_text(r[1]))
        uniq = []
        for c in cands:
            if c not in uniq:
                uniq.append(c)
        uniq += ["nullptr"] * (4 - len(uniq))
        body = ",\n        ".join(c if c == "nullptr" else '"%s"' % c for c in uniq)
        lines.append('    {"%s", {\n        %s}},' % (name, body))
    lines += ["};", "}", ""]
    open(path, "w").write("\n".join(lines))


def main():
    ref, tgt = Macho(sys.argv[1]), Macho(sys.argv[2])
    out = {"functions": {}}
    for name, rva in REF_FUNCS.items():
        out["functions"][name] = {}
        for level, loose in (("strict", False), ("loose", True)):
            r = shortest_unique(ref, tgt, rva, loose=loose)
            if not r:
                out["functions"][name][level] = {"error": "no unique signature"}
                continue
            n, sig, t, uniq = r
            loc = t if uniq is True else None
            out["functions"][name][level] = {"n": n, "unique_in_target": uniq,
                                  "target_rva": loc if isinstance(loc, int) else None,
                                  "target_candidates": t if isinstance(t, list) else [t],
                                  "signature": sig_text(sig)}
    if "--emit-header" in sys.argv:
        emit_header(out, tgt, sys.argv[sys.argv.index("--emit-header") + 1])
    if "--json" in sys.argv:
        open(sys.argv[sys.argv.index("--json") + 1], "w").write(json.dumps(out, indent=1))
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
