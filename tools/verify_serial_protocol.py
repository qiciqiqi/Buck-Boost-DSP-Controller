from pathlib import Path
import subprocess,shutil,os
r=Path(__file__).resolve().parents[1];w=r/'build/serial_protocol_tests';w.mkdir(parents=True,exist_ok=True)
for f in (r/'ccs_project/SerialDriver').iterdir():
 if f.suffix in ['.c','.h']:shutil.copy2(f,w/f.name)
f=w/'serialDriver.c';t=f.read_text();
for field in ['version','revision','maxVariables']:t=__import__('re').sub(r'\.'+field+r'\s*=\s*','',t)
f.write_text(t)
(w/'test.c').write_text('''#include "serialDriver.h"
#include <assert.h>
#include <stdio.h>
extern ____SerialDriver ____serialDriver;
static uint16_t reply[256],nreply;
static uint32_t written;static uint16_t writes;
int viewerMemoryAllowed(uint32_t addr,uint16_t bytes,uint16_t write){return addr==0xa800UL && (bytes==2||bytes==4);}
void viewerMemoryRead(uint32_t addr,uint16_t *out,uint16_t bytes){uint16_t i;uint32_t v=0x12345678UL;assert(addr==0xa800UL);for(i=0;i<bytes;i++){out[i]=v&255;v>>=8;}}
void viewerMemoryWrite(uint32_t addr,uint32_t value,uint16_t bytes){assert(addr==0xa800UL);written=value;writes++;}
void serialDriverSendData(uint16_t *buf,uint16_t size){uint16_t i;for(i=0;i<size;i++)reply[nreply++]=buf[i];}
static void frame(uint16_t cmd,uint16_t len,uint32_t addr,uint32_t value,uint16_t bad){
 uint16_t b[32],n=0,i,crc;
 ____serialDriver.state=____SERIAL_DRIVER_STATE_WAIT_SOF1;nreply=0;
 b[n++]=0xaa;b[n++]=0x55;b[n++]=cmd;b[n++]=(cmd==1)?1:len;
 for(i=0;i<4;i++){b[n++]=addr&255;addr>>=8;}
 if(cmd==1)b[n++]=len;else for(i=0;i<len;i++){b[n++]=value&255;value>>=8;}
 crc=____serialDriverCRC16(b,n);b[n++]=(crc&255)^bad;b[n++]=crc>>8;
 for(i=0;i<n;i++)serialDriverReceiveByte(b[i]);
}
int main(void){uint16_t crc;
 frame(1,4,0xa800UL,0,0);assert(nreply==6 && reply[0]==0x78 && reply[1]==0x56 && reply[2]==0x34 && reply[3]==0x12);crc=____serialDriverCRC16(reply,4);assert(reply[4]==(crc&255)&&reply[5]==(crc>>8));
 frame(1,2,0xa800UL,0,0);assert(nreply==4 && reply[0]==0x78 && reply[1]==0x56);
 frame(2,4,0xa800UL,0x3f800000UL,0);assert(nreply==1 && reply[0]==0 && writes==1 && written==0x3f800000UL);
 frame(2,2,0xa800UL,0xcafe,0);assert(nreply==1 && writes==2 && written==0xcafe);
 frame(2,4,0xa800UL,0,1);assert(nreply==0 && writes==2);
 frame(1,4,0xffffffffUL,0,0);assert(nreply==0);
 frame(1,8,0xa800UL,0,0);assert(nreply==0);
 frame(2,5,0xa800UL,0,0);assert(nreply==0 && writes==2);
 puts("SERIAL_PROTOCOL_TESTS_PASSED: C2000 read/write, CRC, malformed size/address guards");return 0;}
''')
l=Path(os.environ.get('MATLAB_LCC_ROOT','D:/MATLAB/R2023b/sys/lcc64/lcc64'));logs=[]
def run(args):
 q=subprocess.run([str(x) for x in args],cwd=w,capture_output=True,text=True,errors='replace',timeout=60);logs.append(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(logs[-1])
 return q.stdout
for name in ['test','serialDriver']:run([l/'bin/lcc64.exe','-c','-noregistrylookup','-I'+str(l/'include64'),'-Fo'+name+'.obj',name+'.c'])
run([l/'bin/lcclnk64.exe','-L'+str(l/'lib64'),'-o','test.exe','test.obj','serialDriver.obj']);print(run([w/'test.exe']))
(r/'docs/validation/串口协议主机测试.txt').write_text('\n'.join(logs),encoding='utf-8')
