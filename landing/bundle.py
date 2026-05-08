#!/usr/bin/env python

import os
import subprocess
import sys

placeholder = "placeholder" in sys.argv

if not placeholder:
    subprocess.run(["npm", "install"])
    subprocess.run(["npm", "run", "generate"])

ROOT = ".output/public"


def gather(path="/"):
    if placeholder:
        return
    for entry in os.scandir(f"{ROOT}{path}"):
        if entry.is_dir():
            yield from gather(f"{path}{entry.name}/")
        elif entry.is_file():
            yield f"{path}{entry.name}"


def cstr(data):
    if data is None:
        return "nullptr"
    if isinstance(data, str):
        data = data.encode("ascii")
    s = '"'
    for byte in data:
        if byte == ord('"'):
            s += '\\"'
        elif byte == ord("\\"):
            s += "\\\\"
        elif byte == ord("\n"):
            s += "\\n"
        elif byte >= 32 and byte <= 127 and byte != ord("?"):
            s += chr(byte)
        else:
            s += f"\\{byte:03o}"
    s += '"'
    return s


HEADER = """\
#pragma once

// If it wasn't obvious, this is a generated file! See the "landing" directory.
// Do not modify manually.

namespace landing {

/// We need to split the data for large resources into multiple parts, because
/// MSVC imposes an unreasonably short maximum string literal limit of 16k.
/// Come on guys, the past century called, they want their static allocations
/// back.
struct Part {
    /// Length of this data part, in case there are embedded nulls in the
    /// string.
    unsigned int length;

    /// Resource data.
    const char *data;
};

/// Data for a landing page resource to be served from memory.
struct Resource {
    /// Path for which this resource should be served, or nullptr for the
    /// fallback page. The fallback page will always be the last entry in the
    /// array.
    const char *path;

    /// Content type to serve the resource with.
    const char *content_type;

    /// Array of content parts that form the data.
    const Part *parts;
};

"""

FOOTER = """\

} // namespace landing
"""

CONTENT_TYPES = {
    "html": "text/html; charset=utf-8",
    "json": "application/json; charset=utf-8",
    "js": "text/javascript; charset=utf-8",
    "css": "text/css; charset=utf-8",
    "woff2": "font/woff2",
}

# Resource registry as intermediate format before writing.
PART_SIZE = 10 if placeholder else 1024
resources = []
parts = []


def register_resource(path, ctype, data):
    resources.append((path, ctype, len(parts)))
    start = 0
    while start < len(data):
        part = data[start : start + PART_SIZE]
        parts.append(part)
        start += len(part)
    parts.append(None)


# Normal resources.
for path in gather():
    if path in ["/404.html", "/200.html"] or path.startswith("/api/"):
        continue
    with open(f"{ROOT}{path}", "rb") as inf:
        content = inf.read()
    ctype = CONTENT_TYPES[path.split(".")[-1]]
    register_resource(path, ctype, content)

# Fallback resource for undefined routes.
if placeholder:
    content = b"<html><body>Placeholder for landing page!</body></html>"
else:
    with open(f"{ROOT}/200.html", "rb") as inf:
        content = inf.read()
ctype = CONTENT_TYPES["html"]
register_resource(None, ctype, content)

# Write the file.
d = "placeholder" if placeholder else "real"
with open(f"include/{d}/landing_data.h", "w") as outf:
    outf.write(HEADER)

    outf.write("static const Part parts[] = {\n")
    for idx, part in enumerate(parts):
        outf.write(
            f"    {{{0 if part is None else len(part)}, {cstr(part)}}}{',' if idx < len(parts) - 1 else ''}\n"
        )
    outf.write("};\n\n")

    outf.write("static const Resource resources[] = {\n")
    for idx, (path, ctype, first_part) in enumerate(resources):
        outf.write(
            f"    {{{cstr(path)}, {cstr(ctype)}, parts + {first_part}}}{',' if idx < len(resources) - 1 else ''}\n"
        )
    outf.write("};\n\n")

    outf.write(FOOTER)
