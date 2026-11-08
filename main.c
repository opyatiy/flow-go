#include "flow.h"
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
PSP_MODULE_INFO("Flow Go",0,0,13);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
/* In this SDK any negative heap size selects automatic sizing. Its magnitude
 * is ignored: the separate threshold is the actual system-memory reserve. */
PSP_HEAP_SIZE_KB(-1);
PSP_HEAP_THRESHOLD_SIZE_KB(8192);
Game games[MAX_GAMES];int game_count;
int catalog_loading,catalog_revision;char catalog_status[160];
Job jobs[MAX_JOBS];int job_count;
char appdir[256],device[8]="ms0:",catalog_url[1024]="",message[160]="";
volatile int running=1,busy=0,cancel_job=0,refresh_requested=0,active_job=-1;
volatile uint64_t transfer_done=0,transfer_total=0,transfer_started=0,transfer_received=0;
volatile int transfer_phase=0;int worker_ready;
int profile=1,theme=0,dim=45,wallpaper_index=0;
static SceUID mutex;
void lock(void){sceKernelWaitSema(mutex,1,NULL);}void unlock(void){sceKernelSignalSema(mutex,1);}
void logmsg(const char *fmt,...){char path[320];snprintf(path,sizeof path,"%s/flow.log",appdir);FILE *f=fopen(path,"a");if(!f)return;va_list a;va_start(a,fmt);vfprintf(f,fmt,a);va_end(a);fputc('\n',f);fclose(f);}
void save_config(void){FILE *f=fopen("settings.ini.tmp","w");if(!f){snprintf(message,sizeof message,"Не удалось сохранить настройки");return;}fprintf(f,"theme=%d\ndim=%d\nwallpaper=%d\nprofile=%d\ncatalog_url=%s\nmusic=%d\nvolume=%d\nauto_covers=%d\n",theme,dim,wallpaper_index,profile,catalog_url,music_enabled,music_volume,auto_covers);int bad=ferror(f);if(fclose(f))bad=1;if(bad||!atomic_replace("settings.ini.tmp","settings.ini"))snprintf(message,sizeof message,"Ошибка записи настроек");}
void save_jobs(void){FILE *f=fopen("queue.bin.tmp","wb");if(!f)return;int ok=fwrite("FGQ1",1,4,f)==4&&fwrite(&job_count,sizeof job_count,1,f)==1&&fwrite(jobs,sizeof(Job),job_count,f)==(size_t)job_count;if(fclose(f))ok=0;if(ok)atomic_replace("queue.bin.tmp","queue.bin");}
void load_jobs(void){FILE *f=fopen("queue.bin","rb");if(!f)f=fopen("queue.bin.bak","rb");if(!f)return;char magic[4];int count;if(fread(magic,1,4,f)==4&&!memcmp(magic,"FGQ1",4)&&fread(&count,sizeof count,1,f)==1&&count>=0&&count<=MAX_JOBS&&fread(jobs,sizeof(Job),count,f)==(size_t)count){job_count=count;for(int i=0;i<count;i++){jobs[i].game.id[47]=0;jobs[i].game.url[1023]=0;jobs[i].game.name[191]=0;jobs[i].game.kind[7]=0;jobs[i].game.sha[64]=0;jobs[i].game.region[15]=0;jobs[i].error[127]=0;if(!valid_id(jobs[i].game.id)||jobs[i].state<0||jobs[i].state>FAILED){job_count=0;break;}if(jobs[i].state==ACTIVE||jobs[i].state==WAITING)jobs[i].state=PAUSED;}}fclose(f);}
static void config(void){FILE *f=fopen("settings.ini","r");if(!f)f=fopen("settings.ini.bak","r");if(!f)return;char line[1200];while(fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;if(!strncmp(line,"theme=",6))theme=atoi(line+6);else if(!strncmp(line,"dim=",4))dim=atoi(line+4);else if(!strncmp(line,"wallpaper=",10))wallpaper_index=atoi(line+10);else if(!strncmp(line,"profile=",8))profile=atoi(line+8);else if(!strncmp(line,"music=",6))music_enabled=atoi(line+6)!=0;else if(!strncmp(line,"volume=",7))music_volume=atoi(line+7);else if(!strncmp(line,"auto_covers=",12))auto_covers=atoi(line+12)!=0;else if(!strncmp(line,"catalog_url=",12))snprintf(catalog_url,sizeof catalog_url,"%s",line+12);}fclose(f);if(theme<0||theme>3)theme=0;if(dim<0||dim>80)dim=40;if(wallpaper_index<0||wallpaper_index>32)wallpaper_index=0;if(profile<1||profile>99)profile=1;if(music_volume<0||music_volume>100)music_volume=25;}
static int exit_cb(int a,int b,void *c){(void)a;(void)b;(void)c;running=0;cancel_job=1;return 0;}
static int callback_thread(SceSize n,void *p){(void)n;(void)p;int cb=sceKernelCreateCallback("flow-exit",exit_cb,NULL);sceKernelRegisterExitCallback(cb);sceKernelSleepThreadCB();return 0;}
int main(int argc,char **argv){(void)argc;getcwd(appdir,sizeof appdir);if(argv&&argv[0]&&strchr(argv[0],':')){snprintf(appdir,sizeof appdir,"%s",argv[0]);char *slash=strrchr(appdir,'/');if(slash)*slash=0;chdir(appdir);}if(!strncmp(appdir,"ef0:",4))strcpy(device,"ef0:");else if(!strncmp(appdir,"ms0:",4))strcpy(device,"ms0:");mkdir("downloads",0777);mkdir("wallpapers",0777);mkdir("covers",0777);mutex=sceKernelCreateSema("flow-lock",0,1,1,NULL);int cb=sceKernelCreateThread("flow-callback",callback_thread,0x11,4096,0,NULL);if(cb>=0)sceKernelStartThread(cb,0,NULL);config();sources_init();load_jobs();game_count=load_active_catalog();int first_catalog=!game_count;if(first_catalog&&strcmp(catalog_cache,"catalog.tsv")==0){game_count=parse_catalog("catalog.seed.tsv",games,MAX_GAMES);if(game_count<0)game_count=0;if(game_count>0&&!save_catalog_cache(games,game_count))logmsg("Could not save initial catalog cache");}if(!*catalog_url)snprintf(catalog_url,sizeof catalog_url,"https://raw.githubusercontent.com/txy7795679/PSVITA-PKGJ-DATADB/master/PSP_GAMES.tsv");refresh_requested=first_catalog;if(game_count){snprintf(catalog_status,sizeof catalog_status,"Сохранённый каталог: %d игр",game_count);}else{snprintf(catalog_status,sizeof catalog_status,"Первая загрузка каталога...");}int clock_result=scePowerSetClockFrequency(333,333,166);logmsg("CPU clock: result=%d cpu=%d bus=%d",clock_result,scePowerGetCpuClockFrequencyInt(),scePowerGetBusClockFrequencyInt());ui_init();sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);int worker=sceKernelCreateThread("flow-network",network_worker,0x38,384*1024,PSP_THREAD_ATTR_USER,NULL);int worker_result=worker>=0?sceKernelStartThread(worker,0,NULL):worker;worker_ready=worker_result>=0?1:-1;logmsg("Network worker: create=0x%08X start=0x%08X",(unsigned)worker,(unsigned)worker_result);if(worker_ready<0)snprintf(message,sizeof message,"Не удалось запустить загрузчик: 0x%08X",(unsigned)worker_result);int audio=sceKernelCreateThread("flow-music",music_worker,0x16,16384,PSP_THREAD_ATTR_USER,NULL);if(audio>=0)sceKernelStartThread(audio,0,NULL);uint32_t old=0,last_repeat=0;while(running){SceCtrlData pad;sceCtrlPeekBufferPositive(&pad,1);uint32_t now=sceKernelGetSystemTimeLow(),press=pad.Buttons&~old;if(pad.Buttons!=old)last_repeat=now+350000;else if((int32_t)(now-last_repeat)>=0){press|=pad.Buttons&(PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT);last_repeat=now+120000;}old=pad.Buttons;lock();if(press)ui_input(press);ui_frame();unlock();sceDisplayWaitVblankStart();if(busy)scePowerTick(PSP_POWER_TICK_ALL);}cancel_job=1;SceUInt timeout=35000000;if(worker>=0)sceKernelWaitThreadEnd(worker,&timeout);lock();save_jobs();unlock();save_config();SceUInt audio_timeout=2000000;if(audio>=0)sceKernelWaitThreadEnd(audio,&audio_timeout);ui_shutdown();sceKernelExitGame();return 0;}
