import re
import shutil
import sys

PROJ = '10-2 myproject.uvprojx'
BAK = PROJ + '.bak_touch'
shutil.copy(PROJ, BAK)

s = open(PROJ, encoding='utf-8').read()

TMPL = '''            <File>
              <FileName>{n}</FileName>
              <FileType>{t}</FileType>
              <FilePath>{p}</FilePath>
            </File>
'''

blocks = [
    TMPL.format(n='bsp_ft6336.c', t='1', p=r'..\Hardware\bsp_ft6336.c'),
    TMPL.format(n='bsp_ft6336.h', t='5', p=r'..\Hardware\bsp_ft6336.h'),
    TMPL.format(n='lv_port_indev_v9.c', t='1', p=r'..\Hardware\lv_port_indev_v9.c'),
]

m = re.search(r'(<GroupName>Hardware</GroupName>\s*<Files>)', s)
if not m:
    print('!! Hardware group not found')
    sys.exit(1)

start = m.end()
end = s.find('</Files>', start)
s = s[:end] + ''.join(blocks) + s[end:]

open(PROJ, 'w', encoding='utf-8', newline='').write(s)
print('added 3 files to Hardware group')
print('group ok:', s.count('<Group>') == s.count('</Group>'))
