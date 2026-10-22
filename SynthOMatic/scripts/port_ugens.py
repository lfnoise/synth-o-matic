#!/usr/bin/env python3
"""Mechanically port the original Synth-O-Matic unit generator sources.

The original files (THINK C, MacRoman, CR line endings) live in ../SOM-classic.
This script converts the unit generator files into Sources/SOMEngine/ugens,
applying only mechanical changes:

  * MacRoman/CR  ->  UTF-8/LF
  * "synth.h"    ->  "som_internal.h"
  * K&R implicit-int unit generator definitions get an explicit void type
  * HLock/HUnlock calls are dropped (handles became plain pointers)
  * definitions of former globals that now live in the SOMContext are dropped
  * a handful of explicit, commented fixes (see FIXES below)

It is kept in the repository so the provenance of every engine file is clear.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CLASSIC = os.path.join(os.path.dirname(ROOT), "SOM-classic")
OUT = os.path.join(ROOT, "Sources", "SOMEngine", "ugens")

FILES = [
    "ArithOps.c", "LogicOps.c", "Oscil.c", "SourceGens.c", "Controls.c",
    "Filters.c", "Delays.c", "KSString.c", "vosim.c",
]

# former globals now defined in som_context.c or held in the SOMContext
DROP_LINES = [
    r"^unsigned long randnum;\s*$",
    r"^double randfac;\s*$",
    r"^double randfac1;\s*$",
    r"^double nyqlim;\s*$",
    r"^double log001, log01, log1;\s*$",
    r"^double cps2rad;\s*$", r"^double rad2inc;\s*$", r"^double cps2inc;\s*$",
    r"^double rad2inc2;\s*$", r"^double cps2inc2;\s*$", r"^double rad2lfinc;\s*$",
    r"^double cps2lfinc;\s*$", r"^double rad2lfinc2;\s*$", r"^double cps2lfinc2;\s*$",
    r"^double pisr, pi2sr;\s*$",
    r"^\s*HLock\(.*\);\s*$",
    r"^\s*HUnlock\(.*\);\s*$",
]

# (file, old, new, reason)
FIXES = [
    # oscil1a read through an uninitialized pointer instead of the table
    ("Controls.c", "nxtlevel = (v1 + (*tbl - v1) * frac) * *s3;",
     "nxtlevel = (v1 + (s1[pphase] - v1) * frac) * *s3;	/* port: was *tbl, uninitialized */", None),
    # after the one shot ended, only one sample per remaining 32 sample chunk
    # was written, leaving stale data from a reused buffer in the rest
    ("Controls.c", "\t\tfor (; nsmps; nsmps--) {\n\t\t\t*outi++ = s1[BUFLEN-1];",
     "\t\tfor (nsmps <<= 5; nsmps; nsmps--) {	/* port: fill the rest of the block */\n\t\t\t*outi++ = s1[BUFLEN-1];", None),
    # the reverb used QuickDraw to report a problem
    ("Delays.c",
     "\t\t\tGrafPtr savePort;\n\t\t\tGetPort(&savePort);\n\t\t\tSetPort(dataWindow);\n\t\t\tSysBeep(12);\n"
     "\t\t\tdprintf(\"Reverb Loop Size Discrepancy\\r\");\n\t\t\tSetPort(savePort);\n",
     "\t\t\tdprintf(\"Reverb Loop Size Discrepancy\\r\");\n", None),
    # loop lengths were computed into a static array shared by every reverb
    ("Delays.c", "static long ilpt[6];\n", "", None),
    ("Delays.c", "\tlong loopsum, i;\n\tReverbRecPtr p;",
     "\tlong loopsum, i, ilpt[6];\n\tReverbRecPtr p;", None),
    # a delay time of zero gave a zero length delay line
    ("Delays.c", "\t\t\tloopmax = gSampleRate * *s2;\n",
     "\t\t\tloopmax = gSampleRate * *s2;\n\t\t\tloopmax = smax(1, loopmax);	/* port: guard zero length */\n", None),
    ("KSString.c", "\t\t\tloopmax = gSampleRate / this->in[1]->out[0];\n",
     "\t\t\tloopmax = gSampleRate / this->in[1]->out[0];\n\t\t\tloopmax = smax(1, loopmax);	/* port: guard zero length */\n", None),
    ("KSString.c", "\t\t\tloopmax = gSampleRate / (2. * this->in[1]->out[0]);\n",
     "\t\t\tloopmax = gSampleRate / (2. * this->in[1]->out[0]);\n\t\t\tloopmax = smax(1, loopmax);	/* port: guard zero length */\n", None),
]


def convert(name):
    src = open(os.path.join(CLASSIC, name), "rb").read().decode("mac_roman")
    src = src.replace("\r\n", "\n").replace("\r", "\n")
    src = src.replace('#include "synth.h"', '#include "som_internal.h"')

    lines = []
    for line in src.split("\n"):
        if any(re.match(p, line) for p in DROP_LINES):
            continue
        lines.append(line)
    src = "\n".join(lines)

    # int <newline> name(UGen *this)
    src = re.sub(r"^int\s*\n(\w+)\(UGen \*this\)", r"void\n\1(UGen *this)", src, flags=re.M)
    # int name(UGen *this)
    src = re.sub(r"^int\s+(\w+)\(UGen \*this\)", r"void \1(UGen *this)", src, flags=re.M)
    # name(UGen *this) with an implicit int
    src = re.sub(r"(?<!void\n)^(\w+)\(UGen \*this\)", r"void \1(UGen *this)", src, flags=re.M)

    for fname, old, new, _ in FIXES:
        if fname != name:
            continue
        count = src.count(old)
        if count == 0:
            sys.exit(f"fix not applied in {name}: {old[:60]!r}")
        src = src.replace(old, new)

    header = (f"/*\n *  {name}\n *\n *  Ported from the original Synth-O-Matic source by scripts/port_ugens.py.\n"
              " *  Apart from the commented 'port:' fixes the code is as it was written.\n */\n\n")
    if not src.endswith("\n"):
        src += "\n"
    open(os.path.join(OUT, name), "w").write(header + src)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name in FILES:
        convert(name)
        print("ported", name)


if __name__ == "__main__":
    main()
