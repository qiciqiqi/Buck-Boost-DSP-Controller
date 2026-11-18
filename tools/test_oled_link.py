"""Read OLED transfer health over UART with MCUViewer acquisition stopped."""
import argparse
import json
import re
import time
from pathlib import Path
import serial
from test_viewer_link import read_values

p=argparse.ArgumentParser();p.add_argument('--port',default='COM14');args=p.parse_args()
r=Path(__file__).resolve().parents[1]
symbols={n:int(a,16) for a,n in re.findall(r'^0\s+([0-9a-fA-F]+)\s+(\S+)\s*$',(r/'ccs_project/CPU1_FLASH/BUCK_F28P55X.map').read_text(),re.M)}
names=['oled_online','oled_last_error','viewer_rx_bytes','viewer_errors','power_output_enabled']
entries=[(symbols[n],2 if n in ['oled_online','power_output_enabled'] else 4) for n in names]
with serial.Serial(args.port,115200,timeout=.1) as uart:
    samples=[]
    for i in range(12):
        samples.append(dict(zip(names,read_values(uart,entries))));time.sleep(.3)
passed=all(s['oled_online']==1 and s['viewer_errors']==0 and s['power_output_enabled']==0 for s in samples)
report={'passed':passed,'samples':samples}
(r/'docs/validation/20261116_OLED_MCUViewer/OLED持续刷新实测.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
raise SystemExit(0 if passed else 1)
