"""检查 Keil 引用无重复、算法 token 与原件一致及真实 ARM 链接证据。

构建后运行：源文件必须存在且只编译一次，测试夹具/heap 不得入固件。
从最新日志核对零错误警告、AXF 时间和内存大小，从 map 核对端口 IRQ
以及 DMA/固定解析池地址。结果写到 build_summary.json；不代表硬件验证。
"""
from pathlib import Path
import xml.etree.ElementTree as ET
import json
import re
from source_tokens import verify_algorithms
root=Path(__file__).resolve().parents[1]
project=root/'MDK-ARM/STM32F405RGT6.uvprojx'
tree=ET.parse(project); paths=[]
for entry in tree.findall('./Targets/Target/Groups/Group/Files/File'):
    path=entry.findtext('FilePath')
    if path:
        resolved=(project.parent/path.replace('\\','/')).resolve()
        assert resolved.exists(),resolved
        if resolved.suffix in ['.c','.s']:paths.append(resolved)
assert len(paths)==len(set(paths)), 'duplicate compiled implementation'
assert not any('fixtures' in p.parts for p in paths)
assert not any(p.name.startswith('heap_') for p in paths)
algorithm_files=verify_algorithms(root)
assert not (root/'User/boat').exists(), 'legacy algorithm location still present'
assert len(list((root/'App/Control/Boat').glob('*.[ch]')))==algorithm_files
for path in (root/'App/Control/Boat').glob('*.[ch]'):
    source=path.read_text(encoding='utf-8')
    assert not re.search(r'#include\s+"(?:main|FreeRTOS|task|usart)\.h"',source),path
log=(root/'MDK-ARM/freertos_build_final.log').read_text(encoding='utf-8',errors='replace')
assert '0 Error(s), 0 Warning(s)' in log
assert (root/'MDK-ARM/Objects/STM32F405RGT6.axf').stat().st_mtime >= project.stat().st_mtime
size=re.search(r'Code=(\d+) RO-data=(\d+) RW-data=(\d+) ZI-data=(\d+)',log)
assert size
code,ro,rw,zi=map(int,size.groups())
map_text=(root/'MDK-ARM/Objects/STM32F405RGT6.map').read_text(encoding='utf-8',errors='replace')
symbols={}
for name in ['ports','dma_gnss','dma_heading','arena']:
    match=re.search(r'^\s*'+name+r'\s+(0x[0-9a-fA-F]+)',map_text,re.M)
    assert match,name
    address=int(match.group(1),16);assert 0x20000000<=address<0x20020000,(name,hex(address))
    symbols[name]=hex(address)
for name in ['SVC_Handler','PendSV_Handler','SysTick_Handler']:
    assert re.search(r'^\s*'+name+r'\s+0x[0-9a-fA-F]+.*port\.o',map_text,re.M),name
result={'compiled_sources':len(paths),'algorithm_token_match_files':algorithm_files,'algorithm_directory':'App/Control/Boat','armclang':'6.24',
        'freertos':'11.2.0','errors':0,'warnings':0,'code_bytes':code,'ro_bytes':ro,'rw_bytes':rw,
        'zi_bytes':zi,'ram_bytes':rw+zi,'ordinary_sram_symbols':symbols,
        'hardware_validation':'not performed'}
(root/'docs/validation/build_summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
