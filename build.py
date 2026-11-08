"""Build with a PSPDEV toolchain. PSPDEV must point to its installation."""
import os,subprocess,pathlib,sys
root=pathlib.Path(__file__).resolve().parent
sdk=pathlib.Path(os.environ['PSPDEV']).resolve()
os.chdir(root)
os.environ['PATH']=str(sdk/'bin')+os.pathsep+os.environ['PATH']
def run(*args):
    subprocess.run([str(x) for x in args],check=True)
includes=['-I'+str(sdk/'psp/include'),'-I'+str(sdk/'psp/sdk/include'),'-I'+str(sdk/'psp/include/freetype2'),'-Ivendor','-I.']
flags=['-std=gnu11','-O2','-G0','-Wall','-Wextra','-Wno-unused-parameter','-D_PSP_FW_VERSION=660','-DNOUNCRYPT','-DUSE_FILE32API']
run(sys.executable,'build_tls.py')
run(sys.executable,'make_assets.py')
run(sys.executable,'make_buttons.py')
run('psp-gcc','-G0','-c','font_jp.S','-o','font_jp.o')
run('psp-gcc','-G0','-I'+str(sdk/'psp/sdk/include'),'-c','cfw_imports.S','-o','cfw_imports.o')
objs=['font_jp.o','cfw_imports.o']
for src in ['main.c','music.c','ui.c','core.c','scheduler.c','performance.c','sources.c','homebrew.c','install_zip.c','vendor/minizip/unzip.c','vendor/minizip/ioapi.c','catalog.c','tls_fetch.c','network.c','install.c','vendor/pkg2iso.c','vendor/pkgi_aes.c','vendor/aes.c','vendor/sha256.c']:
    obj=src+'.o'
    tlsflags=['-Ivendor/mbedtls3/include','-Ivendor/entropy','-DMBEDTLS_USER_CONFIG_FILE=<tls_config.h>'] if src=='tls_fetch.c' else []
    run('psp-gcc',*tlsflags,*includes,*flags,'-c',src,'-o',obj);objs.append(obj)
lib=sdk/'psp/sdk/lib'
# Resolve newlib's imports before SDK stub archives (PSP import ordering).
objs += ['vendor/libflowtls.a','-Wl,--start-group','-lpthreadglue','-lpthread','-lcglue','-lc','-lgcc','-Wl,--end-group']
run('psp-gcc',*includes,*flags,'-L'+str(sdk/'psp/lib'),'-L'+str(lib),'-specs='+str(lib/'prxspecs'),'-Wl,-q,-T'+str(lib/'linkfile.prx'),'-Wl,-zmax-page-size=128',*objs,str(lib/'prxexports.o'),'-lfreetype','-lbz2','-lpng','-lz','-lm','-lpspaudio','-lpsphttp','-lpspssl','-lpspnet_inet','-lpspnet_resolver','-lpspnet_apctl','-lpspnet','-lpspwlan','-lpsprtc','-lpsppower','-lpsputility','-lpspdebug','-lpspdisplay','-lpspge','-lpspctrl','-o','flow.elf')
run(sys.executable,'tools/verify_heap.py','flow.elf')
run(sys.executable,'tools/verify_tls_clock.py','flow.elf')
run('psp-fixup-imports','flow.elf')
run('psp-prxgen','flow.elf','flow.prx')
run('mksfoex','-d','MEMSIZE=1','Flow Go','PARAM.SFO')
run('pack-pbp','EBOOT.PBP','PARAM.SFO','ICON0.PNG','NULL','NULL','NULL','NULL','flow.prx','NULL')
print('Built',root/'EBOOT.PBP')
