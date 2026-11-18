from pathlib import Path
import configparser,re,argparse,json,os
r=Path(__file__).resolve().parents[1]
a=argparse.ArgumentParser();a.add_argument('--port',default='');a.add_argument('--ofd2000',default=os.environ.get('OFD2000_EXE','D:/software2/ccs_21.0.1/ccs/tools/compiler/ti-cgt-c2000_25.11.1.LTS/bin/ofd2000.exe'));args=a.parse_args()
image=r/'ccs_project/CPU1_FLASH/BUCK_F28P55X.elf';mp=image.with_suffix('.map')
assert image.exists() and image.read_bytes()[:4]==b'\x7fELF'
symbols={name:int(addr,16) for addr,name in re.findall(r'^0\s+([0-9a-fA-F]+)\s+(\S+)\s*$',mp.read_text(),re.M)}
base={
'viewer_heartbeat':5,'viewer_rx_bytes':5,'viewer_tx_bytes':5,'viewer_errors':5,'oled_online':3,
'adc_frame_count':5,'adc_error_count':5,'power_mode':5,'control_mode':5,
'active_power_mode':5,'active_control_mode':5,'power_output_enabled':3,'control_fault_latched':3,
'output_voltage_target':7,'current_reference':7,'openloop_command':7,'feedback_current_avg':7,
'vbat_voltage_avg':7,'vbus_voltage_avg':7,'pwm_duty_counts':7,
'current_limit':7,'pwm_limit':7,'feedback_current':7,'adc_current_raw_avg':7,
'adc_current_raw':3,'adc_reference_raw':3,'adc_vbat_raw':3,'adc_vbus_raw':3,
'oled_last_error':5,'control_isr_max_cycles':5}
for root in ['buck_current_settings','buck_dual_settings','boost_current_settings','boost_dual_settings','buck_voltage_settings']:
 for i,member in enumerate(['current_kp','current_ki','voltage_kp','voltage_ki']):
  name=root+'.'+member;symbols[name]=symbols[root]+2*i;base[name]=7
for root in ['current_pi','voltage_pi','voltage_only_pi']:
 name=root+'.Out';symbols[name]=symbols[root];base[name]=7
assert '____serialDriverSettings' in symbols
common=['power_mode','control_mode','active_power_mode','active_control_mode',
 'current_reference','output_voltage_target','openloop_command',
 'feedback_current_avg','vbat_voltage_avg','vbus_voltage_avg','pwm_duty_counts',
 'power_output_enabled','control_fault_latched','adc_error_count',
 'current_limit','pwm_limit','current_pi.Out','voltage_pi.Out']
tuning_groups={'Modes and Feedback':common,
 'Communication':['viewer_heartbeat','viewer_rx_bytes','viewer_tx_bytes','viewer_errors',
                  'adc_frame_count','adc_error_count','oled_online','oled_last_error','power_output_enabled'],
 'ADC Calibration':['adc_current_raw','adc_current_raw_avg','adc_reference_raw','adc_vbat_raw',
                    'adc_vbus_raw','feedback_current','feedback_current_avg','vbat_voltage_avg',
                    'vbus_voltage_avg','adc_frame_count','adc_error_count','control_isr_max_cycles',
                    'power_output_enabled','control_fault_latched']}
for group in ['buck_current_settings','buck_dual_settings','boost_current_settings','boost_dual_settings','buck_voltage_settings']:
 tuning_groups[group]=[x for x in common if x not in ['adc_error_count','current_limit','pwm_limit']]+[group+'.'+x for x in ['current_kp','current_ki','voltage_kp','voltage_ki']]
 if group=='buck_voltage_settings':tuning_groups[group].append('voltage_only_pi.Out')
assert all(len(xs)<=20 for xs in tuning_groups.values()),'Installed MCUViewer license supports at most 20 active variables'
profiles=[
 ('mcuviewer/check.mcuvproj',{'Communication':[x for x in ['viewer_heartbeat','viewer_rx_bytes','viewer_tx_bytes','viewer_errors','power_output_enabled','oled_online']]}),
 ('mcuviewer/tuning.mcuvproj',{'boost_dual_settings':tuning_groups['boost_dual_settings']})]
for group,variables in tuning_groups.items():
 profiles.append(('mcuviewer/profiles/'+group.replace(' ','_')+'.mcuvproj',{group:variables}))
for filename,groups in profiles:
 cfg=configparser.ConfigParser();cfg.optionxform=str
 cfg['elf']={'file_path':str(image).replace('\\','/'),'relative_path':'false'}
 cfg['general']={'version':'2'}
 cfg['settings']={'sample_frequency_hz':'20','max_points':'10000','max_viewport_points':'5000','viewport_width_ms':'2000',
 'refresh_on_elf_change':'true','stop_acq_on_elf_change':'true','probe_type':'2','target_name':'','probe_mode':'0','probe_sn':'remote',
 'probe_speed_khz':'1000','probe_baudrate':'115200','elf_parser':'c2000',
 'ofd2000_command':str(Path(args.ofd2000).resolve()).replace('\\','/'),
 'gdb_command':'gdb','recorder_enabled':'false','restart_on_group_change':'true','keep_connection':'true','mcp_server_port':'8765',
 'ip_address':'127.0.0.1','port':'3334'}
 names=list(dict.fromkeys(x for xs in groups.values() for x in xs))
 for i,name in enumerate(names):
  assert name in symbols,name
  cfg['var'+str(i)]={'name':name,'tracked_name':name,'address':str(symbols[name]),'type':str(base[name]),'color':'3550450300',
   'should_update_from_elf':'false','shift':'0','mask':'4294967295','high_level_type':'0','value_to_write':'0',
   'secondary_value_to_write':'0','do_not_interpret_as_enum':'false','write_limits_enabled':'false','write_limit_min':'0','write_limit_max':'0'}
 for i,(group,xs) in enumerate(groups.items()):
  cfg['group'+str(i)]={'name':group,'acquisition_type':'0','override_sample_rate':'false','sample_frequency_hz':'20','override_viewport_width':'false','viewport_width_ms':'2000'}
  cfg[f'group{i}-plot0']={'name':group,'type':'2','visibility':'true','secondary_write_enabled':'false'}
  for j,name in enumerate(xs): cfg[f'group{i}-plot0-series{j}']={'name':name,'visibility':'true','format':'DEC'}
 (r/filename).parent.mkdir(exist_ok=True)
 with (r/filename).open('w',encoding='utf-8') as f:cfg.write(f)
(r/'docs/validation/20261116_OLED_MCUViewer').mkdir(parents=True,exist_ok=True)
(r/'docs/validation/20261116_OLED_MCUViewer/变量地址.json').write_text(json.dumps({name:symbols[name] for name in [*base,'____serialDriverSettings']},indent=2),encoding='utf-8')
print('MCUViewer C2000 GDB bridge profiles generated: 127.0.0.1:3334; bridge UART '+args.port+'.')
