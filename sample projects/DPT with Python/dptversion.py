"""Extract DPT API version number from parmref.cpp
"""
import os
import re
import sys

version_line = re.compile(''.join((
    '\s*StoreEntry.*VERSDPT.*?(?P<version>((\d+\.)*\d+))')))

if len(sys.argv) > 1:
    dptpath = sys.argv[1]
else:
    dptpath = os.path.join('..', '..', '..')
    
version = '0.0'
f = open(os.path.join(dptpath, 'source', 'parmref.cpp'),'r')
for t in f.readlines():
    vl = version_line.match(t)
    if vl is not None:
        version = str(vl.group('version'))
        break
f.close()

f = open(os.path.join('python', 'version.py'), 'wb')
f.write(''.join(('_dpt_version = ', "'", version, "'", os.linesep)).encode())
f.close()
