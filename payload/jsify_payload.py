# Axel '0vercl0k' Souchet - 4 May 2019
import sys
import os
import pefile

def main(argc, argv):
    if argc != 2:
        print 'jsify_payload.py <path bin>'
        return 1

    if not os.path.isfile(argv[1]):
        print argv[1], 'does not exist, bailing.'
        return 1

    pe = pefile.PE(argv[1])
    reflective_loader = pe.DIRECTORY_ENTRY_EXPORT.symbols[0]
    assert reflective_loader.name == 'ReflectiveLoader', 'Something is odd!'

    reflective_loader_offset = pe.get_offset_from_rva(reflective_loader.address)
    content = open(argv[1], 'rb').read()
    with open('payload.js', 'w') as fout:
        fout.write('''// Axel '0vercl0k' Souchet - 4 May 2019
const ReflectiveLoaderOffset = 0x%xn;
const ReflectiveDll = new Uint8Array([''' % reflective_loader_offset)
        idx = 0
        while idx < len(content):
            if (idx % 8) == 0:
                fout.write('\n    ')
            fout.write('0x%02x' % ord(content[idx]))
            if (idx + 1) < len(content):
                fout.write(', ')
            idx += 1
        fout.write('''
]);''')
    return 0

if __name__ == '__main__':
    sys.exit(main(len(sys.argv), sys.argv))
