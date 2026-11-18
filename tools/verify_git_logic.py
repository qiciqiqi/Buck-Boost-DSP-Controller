from pathlib import Path
import subprocess,shutil,os
r=Path(__file__).resolve().parents[1];p=r/'ccs_project';w=r/'build/git_logic_tests';w.mkdir(parents=True,exist_ok=True)
for f in list((p/'include').glob('*.h'))+[p/'app_adc.c',p/'app_control.c',p/'pid.c']:shutil.copy2(f,w/f.name)
(w/'platform.h').write_text('''#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
#define POWER_EN 1
#define POWER_PWM_BASE 1
#define POWER_PWM_EPWMA_GPIO 0
#define ADCARESULT_BASE 1
#define ADC_SOC_NUMBER0 0
#define ADC_SOC_NUMBER1 1
#define ADC_SOC_NUMBER2 2
#define ADC_SOC_NUMBER3 3
#define EPWM_COUNTER_COMPARE_A 0
extern unsigned gate,compare;
extern uint16_t raw_adc[4];
static inline void GPIO_writePin(unsigned p,unsigned v){gate=v;}
static inline unsigned GPIO_readPin(unsigned p){return 0;}
static inline unsigned EPWM_getTimeBasePeriod(unsigned p){return 3750;}
static inline void EPWM_setCounterCompareValue(unsigned p,unsigned c,unsigned v){compare=v;}
static inline bool Interrupt_disableGlobal(void){return false;}
static inline void Interrupt_enableGlobal(void){}
static inline uint16_t ADC_readResult(unsigned p,unsigned c){return raw_adc[c];}
void PowerTiming_Start(void);
#endif
''')
(w/'test.c').write_text('''#include "app_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
unsigned gate,compare;uint16_t raw_adc[4]={2076,2000,248,248};
void PowerTiming_Start(void){}
static void tick(void){BuckAdc_ConversionComplete();BuckControl_Tick(0);BuckControl_Tick(1);}
int main(void){unsigned p,c,k;BuckControl_ParameterInit();BuckControl_Start();
assert(power_mode==POWER_BOOST && control_mode==CONTROL_DUAL);
assert(!gate && output_voltage_target==0);
for(p=0;p<2;p++)for(c=0;c<4;c++){
 power_mode=p;control_mode=c;
 for(k=0;k<205;k++)tick();
 assert(!gate && output_voltage_target==0 && current_reference==0 && openloop_command==0);
 if(p==POWER_BOOST && c==CONTROL_VOLTAGE){assert(active_power_mode==MODE_INACTIVE);continue;}
 assert(active_power_mode==p && active_control_mode==c);
 raw_adc[0]=2076;
 if(c==CONTROL_OPEN)openloop_command=10;
 else if(c==CONTROL_CURRENT)current_reference=(p==POWER_BOOST)?.8f:.2f;
 else output_voltage_target=(p==POWER_BOOST)?4:4;
 for(k=0;k<100;k++)tick();
 assert(!control_fault_latched);
 if(p==POWER_BOOST)assert(compare>=1500 && compare<=3563);
 if(c==CONTROL_OPEN){assert(fabs(pwm_duty_counts-375)<.01f);assert(compare==((p==POWER_BOOST)?3375:375));}
 current_reference=output_voltage_target=openloop_command=0;tick();assert(!gate);
}
power_mode=POWER_BUCK;control_mode=CONTROL_OPEN;for(k=0;k<205;k++)tick();
openloop_command=10;tick();assert(gate);
raw_adc[0]=4095;tick();assert(control_fault_latched && !gate);
raw_adc[0]=2076;tick();assert(control_fault_latched && !gate);
openloop_command=0;tick();assert(!control_fault_latched);
openloop_command=NAN;tick();assert(!gate);
power_mode=POWER_BOOST;tick();assert(!gate && active_power_mode==MODE_INACTIVE);
assert(output_voltage_target==0 && openloop_command==0);
power_mode=POWER_BUCK;control_mode=CONTROL_CURRENT;raw_adc[0]=2500;for(k=0;k<205;k++)tick();
current_reference=.2f;tick();assert(pwm_duty_counts==0 && compare==0 && gate==1);
current_reference=0;tick();assert(!gate);
puts("GIT_LOGIC_TESTS_PASSED: seven valid mode pairs, invalid Boost voltage, zero startup, complementary duty, protection, mode reset");return 0;}
''')
l=Path(os.environ.get('MATLAB_LCC_ROOT','D:/MATLAB/R2023b/sys/lcc64/lcc64'));logs=[]
def run(args):
 q=subprocess.run([str(x) for x in args],cwd=w,capture_output=True,text=True,errors='replace',timeout=60);logs.append(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(logs[-1])
 return q.stdout
for name in ['test','app_control','app_adc','pid']:run([l/'bin/lcc64.exe','-c','-noregistrylookup','-I'+str(l/'include64'),'-Fo'+name+'.obj',name+'.c'])
run([l/'bin/lcclnk64.exe','-L'+str(l/'lib64'),'-o','test.exe','test.obj','app_control.obj','app_adc.obj','pid.obj'])
print(run([w/'test.exe']))
(r/'docs/validation/主机逻辑验证.txt').write_text('\n'.join(logs),encoding='utf-8')

