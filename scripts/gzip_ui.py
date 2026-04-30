import gzip
import os

Import("env")

project_dir = env.subst("$PROJECT_DIR")
src = os.path.join(project_dir, "data", "index.html")
dst = os.path.join(project_dir, "include", "WebUiHtml.h")

if os.path.exists(dst) and os.path.exists(src):
    if os.path.getmtime(dst) > os.path.getmtime(src):
        print("[gzip_ui] WebUiHtml.h is up to date, skipping")
        Return()

with open(src, "rb") as f:
    raw = f.read()

compressed = gzip.compress(raw, compresslevel=9)
hex_bytes = ", ".join(f"0x{b:02x}" for b in compressed)

content = (
    "#pragma once\n"
    "// Auto-generated from data/index.html — do not edit\n"
    f"const uint8_t  WEB_UI_HTML_GZ[]    = {{ {hex_bytes} }};\n"
    "const size_t   WEB_UI_HTML_GZ_LEN  = sizeof(WEB_UI_HTML_GZ);\n"
)

os.makedirs(os.path.dirname(dst), exist_ok=True)
with open(dst, "w") as f:
    f.write(content)

print(f"[gzip_ui] {len(raw)} → {len(compressed)} bytes → include/WebUiHtml.h")
