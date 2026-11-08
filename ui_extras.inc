/* Included by ui.c: shares framebuffer/text primitives. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include <pspwlan.h>
#include <pspnet_apctl.h>
#include <pspiofilemgr_devctl.h>
static FT_Library jp_library;static FT_Face jp_face;
typedef struct {int cp,size,width,height,left,top,advance;unsigned char pixels[32*32];} CachedGlyph;
static CachedGlyph jp_cache[256];static unsigned jp_next;
extern const unsigned char flow_jp_start[],flow_jp_end[];
static void jp_init(void){
    int err=FT_Init_FreeType(&jp_library);
    if(!err)err=FT_New_Memory_Face(jp_library,flow_jp_start,flow_jp_end-flow_jp_start,0,&jp_face);
    if(!err)err=FT_Select_Charmap(jp_face,FT_ENCODING_UNICODE);
    logmsg("Embedded Japanese font: result=%d bytes=%u",err,(unsigned)(flow_jp_end-flow_jp_start));
    if(err){if(jp_face)FT_Done_Face(jp_face);jp_face=NULL;snprintf(message,sizeof message,"Ошибка встроенного японского шрифта: %d",err);}
}
static CachedGlyph *jp_glyph(int cp,int size){
    if(!jp_face)return NULL;
    for(int i=0;i<256;i++)if(jp_cache[i].cp==cp&&jp_cache[i].size==size)return &jp_cache[i];
    unsigned index=FT_Get_Char_Index(jp_face,cp);if(!index)return NULL;
    if(FT_Set_Pixel_Sizes(jp_face,0,size)||FT_Load_Glyph(jp_face,index,FT_LOAD_RENDER))return NULL;
    FT_GlyphSlot slot=jp_face->glyph;if(slot->bitmap.width>32||slot->bitmap.rows>32)return NULL;
    CachedGlyph *g=&jp_cache[jp_next++%256];memset(g,0,sizeof *g);g->cp=cp;g->size=size;g->width=slot->bitmap.width;g->height=slot->bitmap.rows;g->left=slot->bitmap_left;g->top=slot->bitmap_top;g->advance=slot->advance.x>>6;
    for(int y=0;y<g->height;y++)memcpy(g->pixels+y*32,slot->bitmap.buffer+y*slot->bitmap.pitch,g->width);
    return g;
}
static void pixel(int x,int y,uint32_t color){if(x>=0&&x<480&&y>=0&&y<272)screen[y*512+x]=color;}
static void stroke(int x0,int y0,int x1,int y1,uint32_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(;;){pixel(x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}
/* Face buttons + D-pad + L/R; START/SELECT use their printed names, independent of font coverage. */
static int is_button(int cp){return cp==0x2715||cp==0x25cb||cp==0x25a1||cp==0x25b3||(cp>=0xe000&&cp<=0xe002);}
#include "buttons.h"
static void button_icon(int cp,int x,int y,int size,uint32_t color){
    int n=size<14?11:14,cx=x+n/2,cy=y+n/2;
    const unsigned char *mask=NULL;
    if(cp==0x2715){mask=n==11?button_cross_11:button_cross_14;color=RGB(108,169,245);}
    else if(cp==0x25cb){mask=n==11?button_circle_11:button_circle_14;color=RGB(240,108,120);}
    else if(cp==0x25a1){mask=n==11?button_square_11:button_square_14;color=RGB(242,154,200);}
    else if(cp==0x25b3){mask=n==11?button_triangle_11:button_triangle_14;color=RGB(99,215,176);}
    if(mask){for(int j=0;j<n;j++)for(int i=0;i<n;i++){int px=x+i,py=y+j,a=mask[j*n+i];if(a&&px>=0&&px<480&&py>=0&&py<272){uint32_t *p=screen+py*512+px;*p=mix(*p,color,a);}}return;}

    if(cp==0x2715){stroke(x+2,y+2,x+n-2,y+n-2,color);stroke(x+n-2,y+2,x+2,y+n-2,color);}
    else if(cp==0x25cb){int r=n/2-1;for(int i=0;i<32;i++){float a=i*6.2831853f/32;pixel(cx+(int)roundf(r*cosf(a)),cy+(int)roundf(r*sinf(a)),color);}}
    else if(cp==0x25a1){stroke(x+1,y+1,x+n-1,y+1,color);stroke(x+1,y+n-1,x+n-1,y+n-1,color);stroke(x+1,y+1,x+1,y+n-1,color);stroke(x+n-1,y+1,x+n-1,y+n-1,color);}
    else if(cp==0x25b3){stroke(cx,y+1,x+1,y+n-1,color);stroke(x+1,y+n-1,x+n-1,y+n-1,color);stroke(x+n-1,y+n-1,cx,y+1,color);}
    else if(cp==0xe000){box(cx-1,y,3,n,color,255);box(x,cy-1,n,3,color,255);}
    else {stroke(x,y+1,x+n,y+1,color);stroke(x,y+1,x,y+n,color);stroke(x+n,y+1,x+n,y+n,color);stroke(x,y+n,x+n,y+n,color);stroke(x+3,y+3,x+3,y+n-2,color);if(cp==0xe001)stroke(x+3,y+n-2,x+n-2,y+n-2,color);else{stroke(x+3,y+3,x+n-3,y+3,color);stroke(x+n-3,y+3,x+n-3,cy,color);stroke(x+3,cy,x+n-3,cy,color);stroke(x+3,cy,x+n-2,y+n-2,color);}}
}
static int tech_page;static unsigned tech_tick;static char tech_labels[6][64],tech_values[6][128];
extern int sctrlHENGetVersion(void),sctrlHENGetMinorVersion(void),kuKernelGetModel(void);
typedef struct {unsigned magic;char path[108],loader[12],launcher[20];unsigned char mode,recovery;} FlowArkConfig;
extern void *sctrlArkGetConfig(FlowArkConfig*);
static void number_or_na(char *out,int value,const char *unit){if(value<0)snprintf(out,128,"API: 0x%08X",(unsigned)value);else snprintf(out,128,"%d %s",value,unit);}
static void tech_refresh(void){
    static int logged;if(!logged){logmsg("Battery API: volts=%d temp=%d remain=%d full=%d lifetime=%d",scePowerGetBatteryVolt(),scePowerGetBatteryTemp(),scePowerGetBatteryRemainCapacity(),scePowerGetBatteryFullCapacity(),scePowerGetBatteryLifeTime());logged=1;}
    memset(tech_labels,0,sizeof tech_labels);memset(tech_values,0,sizeof tech_values);
    if(tech_page==0){
        const char *labels[]={"Заряд","Питание","Напряжение","Температура","Осталось (оценка PSP)","Наличие аккумулятора"};
        for(int i=0;i<6;i++)snprintf(tech_labels[i],64,"%s",labels[i]);
        number_or_na(tech_values[0],scePowerGetBatteryLifePercent(),"%");
        int ac=scePowerIsPowerOnline(),charging=scePowerIsBatteryCharging();snprintf(tech_values[1],128,"%s",charging>0?"Заряжается":ac>0?"Внешнее питание":ac==0?"От аккумулятора":"Недоступно");
        number_or_na(tech_values[2],scePowerGetBatteryVolt(),"мВ");number_or_na(tech_values[3],scePowerGetBatteryTemp(),"°C");number_or_na(tech_values[4],scePowerGetBatteryLifeTime(),"мин");snprintf(tech_values[5],128,"%s",scePowerIsBatteryExist()>0?"Установлен":"Не определён");
    }else if(tech_page==1){
        const char *labels[]={"Системная версия","Модель (система)","Версия CFW","CPU / BUS","Ёмкость: остаток / полная","Версия Flow Go"};for(int i=0;i<6;i++)snprintf(tech_labels[i],64,"%s",labels[i]);
        unsigned fw=sceKernelDevkitVersion();snprintf(tech_values[0],128,"%u.%02u  [0x%08X]",fw>>24,((fw>>16)&255)*10+((fw>>8)&255),fw);int model=kuKernelGetModel();snprintf(tech_values[1],128,"%s / model %d",model==4?"PSP Go":"PSP",model);FlowArkConfig ark={0};sctrlArkGetConfig(&ark);if(ark.magic==0xB00B1E55)snprintf(tech_values[2],128,"ARK 4.%d.%d / %.11s",sctrlHENGetVersion(),sctrlHENGetMinorVersion(),ark.loader);else snprintf(tech_values[2],128,"Не определена");snprintf(tech_values[3],128,"%d / %d МГц",scePowerGetCpuClockFrequencyInt(),scePowerGetBusClockFrequencyInt());int remain=scePowerGetBatteryRemainCapacity(),full=scePowerGetBatteryFullCapacity();if(remain<0||full<0)snprintf(tech_values[4],128,"Нет данных API");else snprintf(tech_values[4],128,"%d / %d мА·ч",remain,full);snprintf(tech_values[5],128,"0.7.3 test");
    }else{
        const char *labels[]={"Накопитель приложения","Свободно на накопителе","Свободная системная RAM","WLAN переключатель","Соединение Wi-Fi","Сохранённый профиль"};for(int i=0;i<6;i++)snprintf(tech_labels[i],64,"%s",labels[i]);snprintf(tech_values[0],128,"%s",device);uint64_t bytes=free_space();if(bytes==UINT64_MAX)snprintf(tech_values[1],128,"Недоступно");else snprintf(tech_values[1],128,"%.2f ГиБ",bytes/(1024.0*1024*1024));snprintf(tech_values[2],128,"%.2f МиБ",sceKernelTotalFreeMemSize()/(1024.0*1024));int wlan=sceWlanGetSwitchState();snprintf(tech_values[3],128,"%s",wlan<0?"Недоступно":wlan?"Включён":"Выключен");snprintf(tech_values[4],128,"%s",network_connected?"Подключено":"Не подключено / нет данных");snprintf(tech_values[5],128,"#%d",profile);
    }
}
static void tech_draw(void){unsigned now=sceKernelGetSystemTimeLow();if(!tech_tick||now-tech_tick>1000000){tech_refresh();tech_tick=now;}const char *names[]={"Аккумулятор","Система","Память и сеть"};char title[100];snprintf(title,sizeof title,"%s    %d / 3",names[tech_page],tech_page+1);text(26,79,14,WHITE,title,420);for(int i=0;i<6;i++){int y=106+i*22;box(22,y,436,21,WHITE,i%2?8:15);text(29,y,11,MUTED,tech_labels[i],230);text(264,y,11,WHITE,tech_values[i],190);}}
