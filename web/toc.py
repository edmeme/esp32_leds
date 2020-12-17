# Compress and embed files to c variables
compress=False
import gzip
import sys
from os import path

if len(sys.argv) != 2:
    print(f"Usage {sys.argv[0]} file")
    exit(1)

infile = sys.argv[1]

f = open(infile,"br")
data = f.read()
f.close()

cdata = gzip.compress(data) if compress else (data + b"\x00")

width=16
name=path.split(path.splitext(infile)[0])[1]
print(f"static const char web_file_{name}[] = {{")
for i in range(0,len(cdata),width):
    for c in cdata[i:i+width]:
        print(f"0x{c:02x},",end="")
    print("")
print("};\n")
