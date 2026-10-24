# -*- coding: utf-8 -*-
"""
Repair vcxproj files damaged by the earlier /utf-8 removal script.

Bug: the regex deleted whole lines including closing tags, e.g.
    <PreprocessorDefinitions>...%(PreprocessorDefinitions)</PreprocessorDefinitions>
became
    <PreprocessorDefinitions>...%(PreprocessorDefinitions)
which breaks XML (mismatched tag) -> VS fails to load the project.

Fix: re-close any tag that lost its </...>.
"""
import re, os

PROJ = [
    r"D:\QTcode\HChat\GateServer\GateServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer\ChatServer.vcxproj",
    r"D:\QTcode\HChat\ChatServer2\ChatServer.vcxproj",
    r"D:\QTcode\HChat\StatusServer\StatusServer.vcxproj",
]

# tag -> the line prefix that must be followed by its closing tag
PAIRS = [
    ("PreprocessorDefinitions", r"<PreprocessorDefinitions>"),
    ("AdditionalOptions",        r"<AdditionalOptions>"),
    ("AdditionalIncludeDirectories", r"<AdditionalIncludeDirectories>"),
    ("AdditionalDependencies",   r"<AdditionalDependencies>"),
    ("AdditionalLibraryDirectories", r"<AdditionalLibraryDirectories>"),
]

TAG_RE = {
    "PreprocessorDefinitions": "PreprocessorDefinitions",
    "AdditionalOptions": "AdditionalOptions",
    "AdditionalIncludeDirectories": "AdditionalIncludeDirectories",
    "AdditionalDependencies": "AdditionalDependencies",
    "AdditionalLibraryDirectories": "AdditionalLibraryDirectories",
}


def read(p):
    raw = open(p, "rb").read()
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return None, None


for p in PROJ:
    txt, enc = read(p)
    if txt is None:
        print("  [skip]", p)
        continue
    nl = "\r\n" if "\r\n" in txt else "\n"
    lines = txt.split(nl)
    out = []
    fixed = 0
    for ln in lines:
        for name, openpat in PAIRS:
            tag = TAG_RE[name]
            o = "<%s>" % tag
            c = "</%s>" % tag
            if o in ln and c not in ln:
                ln = ln + c
                fixed += 1
        out.append(ln)
    txt2 = nl.join(out)
    if fixed:
        open(p, "wb").write(txt2.encode(enc))
    print("  %-14s fixed=%d" % (os.path.basename(os.path.dirname(p)), fixed))

print("done")