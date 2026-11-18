from pathlib import Path
import subprocess, os, shutil

root=Path(__file__).resolve().parents[1]
ccs=Path(os.environ.get('CCS_ROOT','D:/software2/ccs_21.0.1/ccs'))
(root/'docs/validation').mkdir(exist_ok=True)
node=ccs/'tools/node/node.exe'
launchers=list((ccs/'eclipse/plugins').glob('com.ti.ccs.apps_*/scripts/app-launcher.js'))
assert len(launchers)==1,launchers
base=[str(node),str(launchers[0]),'-workspace',str(root/'.ccs21-workspace')]
def run(name,args):
    p=subprocess.run(base+args,capture_output=True,text=True,errors='replace',timeout=180)
    (root/('docs/validation/新版CCS_'+name+'.txt')).write_text(p.stdout+'\n'+p.stderr,encoding='utf-8')
    print(name,p.returncode,p.stdout[-1800:],p.stderr,flush=True)
    assert p.returncode==0
    return p.stdout
sdk=os.environ.get('C2000WARE_ROOT','D:/TI_CCS/C2000Ware_26_02_00_00/C2000Ware_26_02_00_00')
run('产品发现',['-application','initialize','-ccs.productDiscoveryPath',sdk])
for cfg in ['CPU1_RAM','CPU1_FLASH']:
    s=run(cfg,['-application','projectBuild','-ccs.locations',str(root/'ccs_project'),'-ccs.autoImport','-ccs.configuration',cfg,'-ccs.buildType','full','-ccs.listProblems'])
    assert '0 errors' in s and 'Finished building target:' in s
    assert (ccs/'tools/compiler').as_posix() in s.replace('\\','/')
    assert (ccs/'utils/sysconfig_1.28.1').as_posix() in s.replace('\\','/')
    shutil.copy2(root/'ccs_project'/cfg/'BUCK_F28P55X.out',root/'ccs_project'/cfg/'BUCK_F28P55X.elf')
print('NEW_CCS_BOTH_CONFIGURATIONS_PASSED',flush=True)
