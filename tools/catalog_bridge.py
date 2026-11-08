"""Optional LAN bridge for PSP firmware without modern HTTPS. Python 3 only."""
import socket
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SOURCE='https://raw.githubusercontent.com/txy7795679/PSVITA-PKGJ-DATADB/master/PSP_GAMES.tsv'
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path!='/catalog.tsv':
            self.send_error(404);return
        try:
            request=urllib.request.Request(SOURCE,headers={'User-Agent':'FlowGoCatalogBridge/0.3'})
            with urllib.request.urlopen(request,timeout=25) as response:
                data=response.read(4*1024*1024+1)
            if len(data)>4*1024*1024 or not data.startswith(b'Title ID\t'):
                raise ValueError('Unexpected upstream catalog')
        except Exception as exc:
            print('Catalog source failed:',exc)
            self.send_error(502,'Catalog source unavailable');return
        self.send_response(200)
        self.send_header('Content-Type','text/tab-separated-values; charset=utf-8')
        self.send_header('Content-Length',str(len(data)))
        self.send_header('Cache-Control','no-store')
        self.end_headers()
        try:self.wfile.write(data)
        except (BrokenPipeError,ConnectionResetError):pass

if __name__=='__main__':
    sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
    try:
        sock.connect(('192.0.2.1',80));ip=sock.getsockname()[0]
    except OSError:ip='<PC-LAN-IP>'
    finally:sock.close()
    print('Flow Go catalog bridge. Keep this window open. Ctrl+C stops it.')
    print('PSP and PC must be on the same LAN. Allow Python on private networks if prompted.')
    print('Set this line in PSP/GAME/FLOWGO/settings.ini:')
    print('catalog_url=http://'+ip+':8765/catalog.tsv',flush=True)
    try:ThreadingHTTPServer(('0.0.0.0',8765),Handler).serve_forever()
    except KeyboardInterrupt:pass
