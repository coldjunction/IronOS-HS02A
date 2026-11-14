"""Puts the compiled demo into the page as base64, so the result is one file that also opens
from disk. Usage: pack.py <page.html> <demo.wasm> <out.html>"""
import base64
import os
import sys

page, wasm, out = sys.argv[1:4]
html = open(page, encoding="utf-8").read()
assert html.count("@@WASM@@") == 1
html = html.replace("@@WASM@@", base64.b64encode(open(wasm, "rb").read()).decode())
open(out, "w", encoding="utf-8", newline="\n").write(html)
print("wrote", os.path.basename(out), os.path.getsize(out), "bytes")
