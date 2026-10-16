# gen-rom-header.py -- concatenate binary files into a C array body.
import sys

if len(sys.argv) < 2:
    raise SystemExit('usage: gen-rom-header.py FILE...')

for filename in sys.argv[1:]:
    data = open(filename, 'rb').read()
    for i in range(0, len(data), 16):
        print('  ' + ' '.join('0x%02x,' % b for b in data[i:i+16]))
