import os,subprocess,pathlib,concurrent.futures
root=pathlib.Path(__file__).resolve().parent
sdk=pathlib.Path(os.environ['PSPDEV']).resolve();os.chdir(root)
os.environ['PATH']=str(sdk/'bin')+os.pathsep+os.environ['PATH']
base=root/'vendor/mbedtls3';objects=root/'vendor/tls-objects';objects.mkdir(exist_ok=True)
flags=['-std=gnu11','-O2','-G0','-ffunction-sections','-fdata-sections','-I'+str(base/'include'),'-I'+str(base/'library'),'-I.','-Ivendor/entropy','-I'+str(sdk/'psp/sdk/include'),'-DMBEDTLS_USER_CONFIG_FILE=<tls_config.h>']
sources=list((base/'library').glob('*.c'))+list((root/'vendor/entropy').glob('*.c'))
headers=list((base/"include").rglob("*.h"))+list((base/"library").glob("*.h"))+list((root/"vendor/entropy").rglob("*.h"))+[root/"tls_config.h"]
header_time=max(p.stat().st_mtime for p in headers)
def compile(p):
    obj=objects/(p.parent.name+'_'+p.stem+'.o')
    if obj.exists() and obj.stat().st_mtime>max(p.stat().st_mtime,header_time):return obj
    subprocess.run(['psp-gcc',*flags,'-c',str(p),'-o',str(obj)],check=True)
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objs=list(pool.map(compile,sources))
archive=root/'vendor/libflowtls.new.a'
if archive.exists():archive.unlink()
subprocess.run(['psp-ar','rcs',archive.as_posix(),*[p.name for p in objs]],cwd=objects,check=True)
members=subprocess.check_output(['psp-ar','t',archive.as_posix()],text=True).splitlines()
assert len(members)==len(objs) and len(set(members))==len(members),'Duplicate TLS archive members'
archive.replace(root/'vendor/libflowtls.a')
print('Built modern TLS library')
