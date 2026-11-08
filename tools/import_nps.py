"""python import_nps.py PSP_GAMES.tsv catalog.tsv -- import a downloaded NPS list."""
import argparse,csv,re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('input',type=Path);p.add_argument('output',type=Path);args=p.parse_args()
rows=[];seen=set()
with args.input.open(encoding='utf-8-sig',newline='') as f:
    for r in csv.DictReader(f,delimiter='\t'):
        id=r.get('Title ID','').strip();url=r.get('PKG direct link','').strip();name=r.get('Name','').replace('\t',' ').replace('\n',' ');size=r.get('File Size','').strip();sha=r.get('SHA256','').strip()
        if id in seen or not re.fullmatch(r'[A-Za-z0-9_-]{1,40}',id) or not url.startswith(('http://','https://')) or not size.isdigit() or not 0<int(size)<=2147483647 or len(name.encode())>=192 or len(url.encode())>=1024 or not re.fullmatch(r'[a-fA-F0-9]{64}',sha):continue
        seen.add(id);rows.append([id,name,r.get('Region',''),'pkg',size,sha,url])
rows.sort(key=lambda r:(r[1].casefold(),r[2]))
if not rows:raise SystemExit('No usable PSP entries found; existing output not changed.')
if len(rows)>6000:raise SystemExit('More than 6000 entries; filter source first.')
with args.output.open('w',encoding='utf-8',newline='') as f:
    w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(['id','name','region','kind','size','sha256','url']);w.writerows(rows)
print(f'{len(rows)} games -> {args.output}')
