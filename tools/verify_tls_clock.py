"""Regression check against stale archive objects using libc time()."""
import subprocess,sys
for name in ['mbedtls_x509_time_is_future','mbedtls_x509_time_is_past']:
    assembly=subprocess.check_output(['psp-objdump','-d','--disassemble='+name,sys.argv[1]],text=True)
    assert '<'+name+'>:' in assembly
    assert '<time>' not in assembly, name+' still calls the old libc clock'
    assert 'jalr' in assembly, name+' must use the registered clock callback'
members=subprocess.check_output(['psp-ar','t','vendor/libflowtls.a'],text=True).splitlines()
assert len(members)==len(set(members)), 'Duplicate TLS object files'
assert all('/' not in p and '\\' not in p for p in members), 'Use canonical archive names'
print('TLS clock checked in linked ELF; TLS archive has no duplicate objects.')
