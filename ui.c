#include "flow.h"
#include "font.h"
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspctrl.h>
#include <psppower.h>
#include <psputility.h>
#include <png.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#define RGB(r,g,b) ((uint32_t)(r)|((uint32_t)(g)<<8)|((uint32_t)(b)<<16)|0xff000000)
#define WHITE RGB(233,243,255)
#define MUTED RGB(176,200,228)
static uint32_t *vram,*screen,*background,*cover_image;static int flip;
static const int tab_order[]={0,5,1,2,3,4};
static int tab,selected,detail,visible[MAX_GAMES],visible_count,queue_selected,setting;
static char favorite[512][48];static int favorite_count;
static char wallpapers[32][128];static int wallpaper_count;
static char cover_id[48],query[64],edit_query[1024];static int keyboard,keypos,keyboard_page,url_keyboard;
static const char *keypages[]={"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789","abcdefghijklmnopqrstuvwxyz0123456789",".:/?#&=%-_~+@[]!$(),;0123456789      "};
static const char *letters="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static uint32_t mix(uint32_t d,uint32_t s,int a){int r=((d&255)*(255-a)+(s&255)*a)/255,g=(((d>>8)&255)*(255-a)+((s>>8)&255)*a)/255,b=(((d>>16)&255)*(255-a)+((s>>16)&255)*a)/255;return RGB(r,g,b);}
static void box(int x,int y,int w,int h,uint32_t color,int alpha){for(int j=y;j<y+h&&j<272;j++)for(int i=x;i<x+w&&i<480;i++)if(i>=0&&j>=0)screen[j*512+i]=mix(screen[j*512+i],color,alpha);}
static void text(int,int,int,uint32_t,const char*,int);
#include "ui_extras.inc"
static int nextcp(const char **s){const unsigned char *p=(const unsigned char*)*s;unsigned c=*p++;int count=0;if(c>=0xc2&&c<=0xdf){c&=31;count=1;}else if(c>=0xe0&&c<=0xef){c&=15;count=2;}else if(c>=0xf0&&c<=0xf4){c&=7;count=3;}else if(c>=128)c='?';for(int i=0;i<count;i++){if(!*p||(*p&0xc0)!=0x80){*s=(const char*)p;return '?';}c=(c<<6)|(*p++&63);}*s=(const char*)p;return c;}
static void text(int x,int y,int size,uint32_t color,const char *s,int maxw){
    int start=x;const Glyph *gs=size==20?font20_glyphs:size==14?font14_glyphs:font11_glyphs;const unsigned char *data=size==20?font20_data:size==14?font14_data:font11_data;
    while(*s){int cp=nextcp(&s);
        if(is_button(cp)){int advance=(size<14?11:14)+3;if(x+advance>start+maxw)break;button_icon(cp,x,y+4,size,color);x+=advance;continue;}
        int idx=cp>=32&&cp<127?cp-32:cp>=0x400&&cp<0x460?95+cp-0x400:-1;
        if(idx<0){CachedGlyph *g=jp_glyph(cp,size);if(g){if(x+g->advance>start+maxw)break;for(int j=0;j<g->height;j++)for(int i=0;i<g->width;i++){int px=x+g->left+i,py=y+size+2-g->top+j,a=g->pixels[j*32+i];if(a&&px>=0&&px<480&&py>=0&&py<272){uint32_t *p=screen+py*512+px;*p=mix(*p,color,a);}}x+=g->advance;continue;}idx='?'-32;}
        const Glyph *g=&gs[idx];if(x+g->advance>start+maxw)break;
        for(int j=0;j<g->height;j++)for(int i=0;i<g->width;i++){int a=data[g->offset+j*g->width+i];if(a&&x+i>=0&&x+i<480&&y+j>=0&&y+j<272){uint32_t *p=screen+(y+j)*512+x+i;*p=mix(*p,color,a);}}x+=g->advance;
    }
}
static void title_lines(int x,int y,const char *name,int width){
    const char *p=name;
    for(int row=0;row<2&&*p;row++){
        char line[256];size_t bytes=0;int used=0;
        while(*p){const char *begin=p;int cp=nextcp(&p);int advance;
            int idx=cp>=32&&cp<127?cp-32:cp>=0x400&&cp<0x460?95+cp-0x400:-1;
            if(idx>=0)advance=font14_glyphs[idx].advance;
            else{CachedGlyph *g=jp_glyph(cp,14);advance=g?g->advance:font14_glyphs['?'-32].advance;}
            int reserve=row==1?15:0;
            if(used+advance>width-reserve||bytes+(size_t)(p-begin)>=sizeof line-4){p=begin;break;}
            memcpy(line+bytes,begin,p-begin);bytes+=p-begin;used+=advance;
        }
        if(row==1&&*p){memcpy(line+bytes,"...",3);bytes+=3;}
        line[bytes]=0;text(x,y+row*21,14,WHITE,line,width);
    }
}
static uint32_t *loadpng(const char *path,int w,int h){png_image image={0};image.version=PNG_IMAGE_VERSION;if(!png_image_begin_read_from_file(&image,path))return NULL;if(image.width>1920||image.height>1920||!image.width||!image.height){png_image_free(&image);return NULL;}image.format=PNG_FORMAT_RGBA;uint8_t *pixels=malloc(PNG_IMAGE_SIZE(image));uint32_t *out=malloc(w*h*4);if(!pixels||!out){free(pixels);free(out);png_image_free(&image);return NULL;}if(!png_image_finish_read(&image,NULL,pixels,0,NULL)){free(pixels);free(out);png_image_free(&image);return NULL;}int contain=w==100&&h==136;float scale=contain?fminf((float)w/image.width,(float)h/image.height):fmaxf((float)w/image.width,(float)h/image.height);float ox=(image.width-w/scale)/2,oy=(image.height-h/scale)/2;for(int y=0;y<h;y++)for(int x=0;x<w;x++){float fx=ox+x/scale,fy=oy+y/scale;if(fx<0||fy<0||fx>=image.width||fy>=image.height){out[y*w+x]=RGB(20,34,55);continue;}unsigned sx=(unsigned)fx,sy=(unsigned)fy;uint8_t *p=pixels+(sy*image.width+sx)*4;out[y*w+x]=mix(RGB(20,34,55),RGB(p[0],p[1],p[2]),p[3]);}free(pixels);png_image_free(&image);return out;}
static int compare_names(const void *a,const void *b){return strcmp(a,b);}
void refresh_background(void){if(!background)return;uint32_t *custom=NULL;if(wallpaper_index>0&&wallpaper_index<=wallpaper_count){char p[256];snprintf(p,sizeof p,"wallpapers/%s",wallpapers[wallpaper_index-1]);custom=loadpng(p,480,272);if(!custom)snprintf(message,sizeof message,"Не удалось открыть PNG обои");}const int palettes[4][6]={{21,38,75,37,78,120},{33,27,64,81,70,118},{16,47,57,36,95,102},{23,27,36,59,70,85}};for(int y=0;y<272;y++)for(int x=0;x<480;x++){int a=(x+y)*255/750;const int *p=palettes[theme];background[y*480+x]=custom?custom[y*480+x]:mix(RGB(p[0],p[1],p[2]),RGB(p[3],p[4],p[5]),a);}free(custom);for(int n=0;n<7;n++)for(int x=0;x<480;x++){int y=(int)(170-30*sinf(x/83.0f)+n*4);background[y*480+x]=mix(background[y*480+x],RGB(174,221,255),50-n*5);}for(int i=0;i<480*272;i++)background[i]=mix(background[i],RGB(0,0,0),dim*255/100);}
static int is_favorite(const char *id){for(int i=0;i<favorite_count;i++)if(!strcmp(id,favorite[i]))return 1;return 0;}
static void toggle_favorite(const char *id){int found=-1;for(int i=0;i<favorite_count;i++)if(!strcmp(id,favorite[i]))found=i;if(found>=0){memmove(favorite[found],favorite[found+1],(favorite_count-found-1)*sizeof favorite[0]);favorite_count--;}else if(favorite_count<512)snprintf(favorite[favorite_count++],48,"%s",id);FILE *f=fopen("favorites.txt.tmp","w");if(f){for(int i=0;i<favorite_count;i++)fprintf(f,"%s\n",favorite[i]);int bad=ferror(f);if(fclose(f))bad=1;if(!bad)atomic_replace("favorites.txt.tmp","favorites.txt");}}
static int contains(const char *s,const char *q){if(!*q)return 1;size_t n=strlen(q);for(;*s;s++)if(!strncasecmp(s,q,n))return 1;return 0;}
static void filter(void){visible_count=0;if((tab==0&&source_is_homebrew())||(tab==5&&!source_is_homebrew()))return;for(int i=0;i<game_count;i++)if((tab!=1||is_favorite(games[i].id))&&contains(games[i].name,query))visible[visible_count++]=i;if(selected>=visible_count)selected=visible_count?visible_count-1:0;}
static void cover(Game *g,int x,int y){static int seen_revision=-1;static unsigned dwell;static char attempted[48];if(seen_revision!=cover_revision){cover_id[0]=0;attempted[0]=0;seen_revision=cover_revision;}if(strcmp(cover_id,g->id)){free(cover_image);char p[128];snprintf(p,sizeof p,"covers/%s.png",g->id);cover_image=loadpng(p,100,136);snprintf(cover_id,sizeof cover_id,"%s",g->id);dwell=sceKernelGetSystemTimeLow();}if(!cover_image&&auto_covers&&!busy&&sceKernelGetSystemTimeLow()-dwell>800000&&strcmp(attempted,g->id)){request_cover(g->id);snprintf(attempted,sizeof attempted,"%s",g->id);}if(cover_image){for(int j=0;j<136;j++)memcpy(screen+(y+j)*512+x,cover_image+j*100,400);}else{box(x,y,100,136,RGB(80,102,133),180);text(x+9,y+8,11,WHITE,"PSP",90);const char *end=g->name;nextcp(&end);char initial[5]={0};size_t initial_size=end-g->name;if(initial_size>4)initial_size=4;memcpy(initial,g->name,initial_size);text(x+35,y+47,20,WHITE,initial,60);text(x+8,y+110,11,MUTED,g->id,85);}}
static void size_label(uint64_t n,char *out,size_t cap){if(!n)snprintf(out,cap,"Размер неизвестен");else if(n>=1024ULL*1024*1024)snprintf(out,cap,"%.2f ГиБ",(double)n/(1024*1024*1024));else snprintf(out,cap,"%.1f МиБ",(double)n/(1024*1024));}
#include "download_ui.inc"
void ui_init(void){tab=source_is_homebrew()?5:0;jp_init();vram=(uint32_t*)((unsigned)sceGeEdramGetAddr()|0x40000000);background=malloc(480*272*4);if(!background)sceKernelExitGame();DIR *d=opendir("wallpapers");struct dirent *ent;if(d){while((ent=readdir(d))&&wallpaper_count<32){size_t n=strlen(ent->d_name);if(n>4&&n<128&&!strcasecmp(ent->d_name+n-4,".png"))snprintf(wallpapers[wallpaper_count++],128,"%s",ent->d_name);}closedir(d);qsort(wallpapers,wallpaper_count,128,compare_names);}if(wallpaper_index>wallpaper_count)wallpaper_index=0;FILE *f=fopen("favorites.txt","r");if(!f)f=fopen("favorites.txt.bak","r");if(f){char line[128];while(fgets(line,sizeof line,f)&&favorite_count<512){line[strcspn(line,"\r\n")]=0;if(valid_id(line))strcpy(favorite[favorite_count++],line);}fclose(f);}refresh_background();sceDisplaySetMode(0,480,272);}
void ui_shutdown(void){free(background);free(cover_image);if(jp_face)FT_Done_Face(jp_face);if(jp_library)FT_Done_FreeType(jp_library);}
static void sync_catalog(void){static int seen;if(seen!=catalog_revision){seen=catalog_revision;selected=0;detail=0;cover_id[0]=0;}}
void ui_frame(void){sync_catalog();screen=vram+flip*512*272;for(int y=0;y<272;y++)memcpy(screen+y*512,background+y*480,480*4);text(21,5,20,WHITE,"flow",70);text(91,14,11,MUTED,source_name(),200);char line[160];int battery=scePowerGetBatteryLifePercent();snprintf(line,sizeof line,"%s  |  %d%%",busy?"Загрузка":"Flow Go 0.7.3",battery<0?0:battery);text(315,14,11,MUTED,line,150);const char *tabs[]={"PSP","Homebrew","Избранное","Загрузки","Настройки","О PSP"};for(int i=0;i<6;i++){int active=tab==tab_order[i];if(active)box(12+i*77,43,76,30,WHITE,30);text(16+i*77,49,11,active?WHITE:MUTED,tabs[i],72);}filter();
if(tab<2||tab==5){
    if(!visible_count){
        const char *title=((tab==0&&source_is_homebrew())||(tab==5&&!source_is_homebrew()))?"Переключение каталога...":!game_count?(catalog_loading||refresh_requested?"Загрузка каталога...":"Каталог не загружен"):*query?"Ничего не найдено":tab==1?"Избранное пока пусто":"Список пуст";
        text(27,108,20,WHITE,title,425);
        if(!game_count){text(27,146,11,MUTED,catalog_status,425);text(27,176,14,WHITE,"✕ Загрузить   START Повторить",425);}
        else if(*query){text(27,149,14,MUTED,"SELECT Изменить поиск",425);}
        else if(tab==1){text(27,149,14,MUTED,"Добавляй игры кнопкой □ в каталоге",425);text(27,179,11,MUTED,"L / R — перейти в другой раздел",425);}
    }else{
        Game *g=&games[visible[selected]];cover(g,24,91);
        title_lines(143,85,g->name,310);
        snprintf(line,sizeof line,"%s  /  %s",g->region,g->id);text(144,133,11,MUTED,line,310);
        size_label(g->size,line,sizeof line);text(144,153,14,WHITE,line,310);
        text(144,178,11,MUTED,is_favorite(g->id)?"□ Убрать из избранного":"□ Добавить в избранное",310);
        box(140,196,313,27,WHITE,detail?40:18);
        text(149,199,14,WHITE,detail?"✕ Подтвердить загрузку   ○ Отмена":"✕ Скачать   △ Обложка",297);
        snprintf(line,sizeof line,"%d / %d",selected+1,visible_count);text(24,228,11,MUTED,line,105);
        text(144,228,11,MUTED,"Влево / вправо: игра   Вверх / вниз: ±10",310);
    }
}
else if(tab==2){
    if(queue_selected>=job_count)queue_selected=job_count?job_count-1:0;
    if(!job_count){text(27,113,20,WHITE,"Очередь пуста",420);text(27,151,11,MUTED,"Выбери игру в каталоге и нажми ✕",420);}
    else{if(queue_selected>=job_count)queue_selected=job_count-1;int first=queue_selected>2?queue_selected-2:0;const char *states[]={"В очереди","Загрузка","Пауза","Готово","Ошибка"};
        for(int i=first;i<job_count&&i<first+3;i++){int y=87+(i-first)*30;box(22,y,436,28,WHITE,i==queue_selected?40:10);text(29,y+2,11,WHITE,jobs[i].game.name,300);text(342,y+2,11,MUTED,(i==active_job&&cancel_job?(delete_active?"Удаление...":"Остановка..."):states[jobs[i].state]),108);}
        Job *j=&jobs[queue_selected];if(j->state==ACTIVE)download_details();
        else if(j->state==FAILED){text(27,193,11,WHITE,j->error,425);text(27,221,11,MUTED,"✕ Повторить   □ Удалить",425);}
        else{if(j->state==WAITING)text(27,185,11,WHITE,worker_ready<0?"Загрузчик не запущен; см. flow.log":active_job>=0?"Ожидает предыдущую игру":busy?"Завершается фоновый сетевой запрос...":"Запуск скачивания...",425);text(27,213,11,MUTED,"✕ Пауза / продолжить   □ Удалить",425);}
    }
}
else if(tab==3){const char *themes[]={"Blue wave","Amethyst","Aurora","Graphite"};const char *labels[]={"Встроенные обои","Своя картинка","Затемнение","Wi-Fi профиль","Обновить каталог","Фоновая музыка","Громкость музыки","Загрузка обложек","Источник PSP","Файл источников"};char values[10][128];snprintf(values[0],128,"%s",themes[theme]);snprintf(values[1],128,"%s",wallpaper_index?wallpapers[wallpaper_index-1]:"Выключена");snprintf(values[2],128,"%d%%",dim);netData data;snprintf(values[3],128,"#%d",profile);if(sceUtilityGetNetParam(profile,PSP_NETPARAM_NAME,&data)>=0)snprintf(values[3],128,"#%d %.70s",profile,data.asString);snprintf(values[4],128,"%s",catalog_loading?"Загружается...":"✕ Обновить");snprintf(values[5],128,"%s",*music_status?music_status:music_enabled?"Включена":"Выключена");snprintf(values[6],128,"%d%%",music_volume);snprintf(values[7],128,"%s",auto_covers?"Автоматически":"Только локальные");snprintf(values[8],128,"%s",source_name());snprintf(values[9],128,"/flow-sources.txt");int first=setting>4?setting-4:0;for(int i=first;i<10&&i<first+5;i++){int y=91+(i-first)*27;box(22,y,436,25,WHITE,setting==i?40:10);text(29,y+1,11,WHITE,labels[i],250);text(286,y+1,11,MUTED,values[i],165);}snprintf(line,sizeof line,"%d / 10",setting+1);text(407,228,11,MUTED,line,60);text(29,228,11,MUTED,catalog_status,365);}
else tech_draw();

if(keyboard){box(13,78,454,163,RGB(10,23,43),245);size_t editlen=strlen(edit_query);snprintf(line,sizeof line,"%s: %.100s","Поиск",edit_query+(editlen>52?editlen-52:0));text(25,84,14,WHITE,line,425);for(int i=0;i<36;i++){int x=26+(i%12)*36,y=117+(i/12)*28;if(i==keypos)box(x-2,y-1,29,25,WHITE,70);char c[2]={letters[i],0};text(x+6,y,14,WHITE,c,24);}text(25,211,11,MUTED,"✕ Ввод   □ Стереть   △ Регистр   START OK",425);}
box(0,246,480,26,RGB(8,20,39),140);const char *hint=tab==2?"✕ Пауза / продолжить   □ Удалить   ○ Назад":tab==4?"L / R Разделы   Вверх / вниз: страницы":tab==3?"L / R Разделы   ✕ Изменить   START Обновить":"L / R Разделы   SELECT Поиск   START Обновить";text(20,248,11,WHITE,*message?message:hint,442);sceDisplaySetFrameBuf(screen,512,PSP_DISPLAY_PIXEL_FORMAT_8888,PSP_DISPLAY_SETBUF_NEXTFRAME);flip^=1;}
static void enqueue(Game *g){if(worker_ready<0){snprintf(message,sizeof message,"Загрузчик не запущен; см. flow.log");return;}for(int i=0;i<job_count;i++)if(!strcmp(jobs[i].game.id,g->id)){tab=2;queue_selected=i;detail=0;return;}if(job_count>=MAX_JOBS){snprintf(message,sizeof message,"Очередь заполнена: удали завершённые задачи");return;}Job *j=&jobs[job_count];memset(j,0,sizeof *j);j->game=*g;j->state=WAITING;queue_selected=job_count++;queue_wake();save_jobs();tab=2;detail=0;}
void ui_input(uint32_t b){sync_catalog();message[0]=0;filter();if(keyboard){if(b&PSP_CTRL_TRIANGLE){keyboard_page=(keyboard_page+1)%3;letters=keypages[keyboard_page];}if(b&PSP_CTRL_LEFT)keypos=(keypos+35)%36;if(b&PSP_CTRL_RIGHT)keypos=(keypos+1)%36;if(b&PSP_CTRL_UP)keypos=(keypos+24)%36;if(b&PSP_CTRL_DOWN)keypos=(keypos+12)%36;size_t n=strlen(edit_query);if(b&PSP_CTRL_CROSS&&n<(url_keyboard?sizeof edit_query-1:sizeof query-1)){edit_query[n]=letters[keypos];edit_query[n+1]=0;}if(b&PSP_CTRL_SQUARE&&n)edit_query[n-1]=0;if(b&PSP_CTRL_LTRIGGER&&n<(url_keyboard?sizeof edit_query-1:sizeof query-1)){edit_query[n]=' ';edit_query[n+1]=0;}if(b&PSP_CTRL_START){snprintf(query,sizeof query,"%.63s",edit_query);selected=0;keyboard=0;detail=0;}if(b&PSP_CTRL_CIRCLE){keyboard=0;url_keyboard=0;}return;}if(b&PSP_CTRL_CIRCLE){detail=0;return;}if(b&(PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER)){int index=0;while(index<5&&tab_order[index]!=tab)index++;tab=tab_order[(index+((b&PSP_CTRL_LTRIGGER)?5:1))%6];if(tab==0||tab==5)source_section(tab==5);tech_tick=0;detail=0;selected=0;query[0]=0;return;}if(b&PSP_CTRL_START&&tab!=4){if(!busy)refresh_requested=1;else snprintf(message,sizeof message,"Дождись завершения текущей операции");return;}if(b&PSP_CTRL_SELECT&&(tab<2||tab==5)){keyboard=1;url_keyboard=0;keyboard_page=0;letters=keypages[0];strcpy(edit_query,query);keypos=0;return;}int delta=b&PSP_CTRL_UP?-1:b&PSP_CTRL_DOWN?1:0;if(tab<2||tab==5){if(!game_count&&(b&PSP_CTRL_CROSS)){if(!busy)refresh_requested=1;return;}if(visible_count){int step=(b&PSP_CTRL_LEFT)?-1:(b&PSP_CTRL_RIGHT)?1:delta*10;if(step){selected=(selected+step)%visible_count;if(selected<0)selected+=visible_count;detail=0;}Game *g=&games[visible[selected]];if(b&PSP_CTRL_SQUARE)toggle_favorite(g->id);if(b&PSP_CTRL_TRIANGLE){request_cover(g->id);snprintf(message,sizeof message,"Обложка запрошена; загрузка после очереди");}if(b&PSP_CTRL_CROSS){if(detail)enqueue(g);else detail=1;}}}
else if(tab==2){if(!job_count)return;queue_selected=(queue_selected+delta+job_count)%job_count;Job *j=&jobs[queue_selected];if(b&PSP_CTRL_CROSS)queue_toggle(queue_selected);if(b&PSP_CTRL_SQUARE){queue_remove(queue_selected);if(queue_selected>=job_count)queue_selected=job_count?job_count-1:0;}}

else if(tab==4){if(delta){tech_page=(tech_page+delta+3)%3;tech_tick=0;}}else{setting=(setting+delta+10)%10;if(b&PSP_CTRL_CROSS){if(setting==0){theme=(theme+1)%4;wallpaper_index=0;}else if(setting==1){wallpaper_index=(wallpaper_index+1)%(wallpaper_count+1);if(!wallpaper_count)snprintf(message,sizeof message,"Добавь PNG в FLOWGO/wallpapers");}else if(setting==2)dim=dim>=80?0:(dim+10>80?80:dim+10);else if(setting==3){if(busy){snprintf(message,sizeof message,"Сначала останови загрузку");return;}int next=profile;for(int i=0;i<99;i++){next=next%99+1;if(sceUtilityCheckNetParam(next)>=0){profile=next;break;}}}else if(setting==4){if(!busy)refresh_requested=1;}else if(setting==5){music_enabled=!music_enabled;music_status[0]=0;}else if(setting==6)music_volume=(music_volume+10)>100?0:music_volume+10;else if(setting==7)auto_covers=!auto_covers;else if(setting==8)source_next();else if(setting==9){snprintf(message,sizeof message,"Редактируй /flow-sources.txt на ПК; затем перезапусти Flow");}save_config();refresh_background();}}}
