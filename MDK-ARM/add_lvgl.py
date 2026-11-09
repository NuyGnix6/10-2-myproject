import os
import re
import shutil
import sys

PROJ = '10-2 myproject.uvprojx'
BAK = PROJ + '.bak_lvgl'

if not os.path.exists(BAK):
    shutil.copy(PROJ, BAK)
    print('backup ->', BAK)

s = open(PROJ, encoding='utf-8').read()

FILE_TMPL = '''            <File>
              <FileName>{name}</FileName>
              <FileType>{ftype}</FileType>
              <FilePath>{path}</FilePath>
            </File>
'''


def fblock(name, path, ftype='1'):
    return FILE_TMPL.format(name=name, ftype=ftype, path=path)


def remove_file(text, path):
    pat = re.compile(
        r'\s*<File>\s*<FileName>[^<]*</FileName>\s*<FileType>[^<]*</FileType>\s*'
        r'<FilePath>' + re.escape(path) + r'</FilePath>\s*</File>')
    return pat.sub('', text, count=1)


def add_to_group(text, group_name, blocks):
    m = re.search(r'(<GroupName>' + re.escape(group_name) + r'</GroupName>\s*<Files>)', text)
    if not m:
        print('!! group not found:', group_name)
        return text
    start = m.end()
    end = text.find('</Files>', start)
    if end < 0:
        print('!! </Files> not found for', group_name)
        return text
    return text[:end] + ''.join(blocks) + text[end:]


# ---------- 1. 移除冲突文件 ----------
s = remove_file(s, r'..\mydriver\LCD.c')
s = remove_file(s, r'..\mydriver\LCD.h')
s = remove_file(s, r'..\Tasks\DisplayTask.c')
s = remove_file(s, r'..\Tasks\DisplayTask.h')
print('removed: LCD.c / LCD.h / DisplayTask.c / DisplayTask.h')

# ---------- 2. Hardware / Tasks 补文件 ----------
s = add_to_group(s, 'Hardware', [
    fblock('bsp_ili9341.c', r'..\Hardware\bsp_ili9341.c'),
    fblock('bsp_ili9341.h', r'..\Hardware\bsp_ili9341.h', '5'),
    fblock('lv_port_disp_v9.c', r'..\Hardware\lv_port_disp_v9.c'),
])

s = add_to_group(s, 'Tasks', [
    fblock('LVGLTask.c', r'..\Tasks\LVGLTask.c'),
    fblock('LVGLTask.h', r'..\Tasks\LVGLTask.h', '5'),
])

# ---------- 3. 扫描 LVGL 所有 .c，放一个大组 ----------
LVGL_SRC = os.path.join('..', 'Middlewares', 'LVGL', 'lvgl', 'src')
if not os.path.isdir(LVGL_SRC):
    print('!! LVGL src not found:', LVGL_SRC)
    sys.exit(1)

lv_files = []
for dirpath, dirnames, filenames in os.walk(LVGL_SRC):
    dirnames.sort()
    for f in sorted(filenames):
        if f.endswith('.c'):
            full = os.path.normpath(os.path.join(dirpath, f))
            lv_files.append(fblock(f, full))

print('lvgl .c files:', len(lv_files))

new_xml = ('        <Group>\n'
           '          <GroupName>LVGL</GroupName>\n'
           '          <Files>\n' + ''.join(lv_files) +
           '          </Files>\n'
           '        </Group>\n')

# UI 组
ui_files = [
    fblock('myui_rewriter.c', r'..\Core\Src\ui\myui_rewriter.c'),
    fblock('myui_rewriter.h', r'..\Core\Src\ui\myui_rewriter.h', '5'),
    fblock('lv_font_ui_18.c', r'..\Core\Src\ui\Font\lv_font_ui_18.c'),
    fblock('lv_font_ui_26.c', r'..\Core\Src\ui\Font\lv_font_ui_26.c'),
    fblock('lv_font_num_32.c', r'..\Core\Src\ui\Font\lv_font_num_32.c'),
]
new_xml += ('        <Group>\n'
            '          <GroupName>UI</GroupName>\n'
            '          <Files>\n' + ''.join(ui_files) +
            '          </Files>\n'
            '        </Group>\n')

idx = s.rfind('</Groups>')
if idx < 0:
    print('!! </Groups> not found')
    sys.exit(1)
s = s[:idx] + new_xml + s[idx:]
print('added 2 groups')

# ---------- 4. IncludePath（文件里有多个，空的跳过，非空的全部追加）----------
ADD = ['../Middlewares/LVGL',
       '../Middlewares/LVGL/lvgl',
       '../Middlewares/LVGL/lvgl/src',
       '../Core/Src/ui']

ms = list(re.finditer(r'<IncludePath>([^<]*)</IncludePath>', s))
for m in reversed(ms):
    if not m.group(1).strip():
        continue
    parts = [p for p in m.group(1).split(';') if p.strip()]
    for a in ADD:
        if a not in parts:
            parts.append(a)
    s = s[:m.start(1)] + ';'.join(parts) + s[m.end(1):]
    print('include path updated (idx chars %d-%d)' % (m.start(1), m.end(1)))

open(PROJ, 'w', encoding='utf-8', newline='').write(s)
print('done, size =', len(s))
