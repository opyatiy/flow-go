"""Host-only queue/source regression test; all writes stay in a temp directory."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cc', nargs='+', default=['cc'], help='Compiler command, e.g. --cc zig cc')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix='flow-go-test-') as temp:
    work = pathlib.Path(temp)
    (work/'disk').mkdir()
    (work/'downloads').mkdir()
    exe = work/'queue-test.exe'
    sources = ['tests/source_switch.c', 'sources.c', 'scheduler.c', 'core.c']
    subprocess.run(args.cc + ['-std=c11', '-O1', '-I'+str(root)] +
                   [str(root/p) for p in sources] + ['-o', str(exe)], check=True)
    subprocess.run([str(exe)], cwd=work, check=True)
