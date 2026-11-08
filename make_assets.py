from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import struct, math
root=Path(__file__).resolve().parent
chars=list(range(32,127))+list(range(0x400,0x460))
out=['/* Noto Sans glyphs; font licensed under SIL OFL, see assets/OFL.txt. */', '#include <stdint.h>','typedef struct {unsigned int cp,offset; unsigned char width,height,advance;} Glyph;']
for size in (11,14,20):
    font=ImageFont.truetype(str(root/'assets/NotoSans.ttf'),size)
    data=bytearray(); glyphs=[]
    for cp in chars:
        ch=chr(cp);width=max(1,math.ceil(font.getlength(ch))+1);height=size+7
        im=Image.new('L',(width,height));ImageDraw.Draw(im).text((0,0),ch,font=font,fill=255,stroke_width=0)
        glyphs.append((cp,len(data),width,height,round(font.getlength(ch))))
        data.extend(im.tobytes())
    assert len(data)<1048576
    out.append('static const unsigned char font%d_data[]={%s};'%(size,','.join(map(str,data))))
    out.append('static const Glyph font%d_glyphs[]={%s};'%(size,','.join('{%d,%d,%d,%d,%d}'%g for g in glyphs)))
(root/'font.h').write_text('\n'.join(out))
icon=Image.new('RGB',(144,80),'#173052');d=ImageDraw.Draw(icon)
for i in range(5):
    pts=[(x,54+12*math.sin(x/60+i/4)+i*2) for x in range(144)]
    d.line(pts,fill=(60+i*10,105+i*10,150+i*10),width=1)
d.text((15,13),'flow / go',font=ImageFont.truetype(str(root/'assets/NotoSans.ttf'),23),fill='white')
icon.save(root/'ICON0.PNG')
