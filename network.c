#include "flow.h"
#include "vendor/sha256.h"
#include <pspkernel.h>
#include <psputility.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psphttp.h>
#include <pspssl.h>
#include <pspwlan.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

volatile int network_connected=0;
int auto_covers=1,cover_revision=0;
static char cover_pending[48];
void clear_cover_request(void){cover_pending[0]=0;}
void request_cover(const char *id){if(valid_id(id))snprintf(cover_pending,sizeof cover_pending,"%s",id);}
static int ready=0;static char failure[128];
static int fail(const char *s){snprintf(failure,sizeof failure,"%s",s);logmsg("%s",s);return 0;}
static int check(int r,const char *where){if(r>=0)return 1;snprintf(failure,sizeof failure,"%s: 0x%08X",where,(unsigned)r);logmsg("%s",failure);return 0;}
uint64_t free_space(void){struct {unsigned max,free,max2,sector,per_cluster;} info={0};void *ptr=&info;int r=sceIoDevctl(device,0x02425818,&ptr,sizeof ptr,NULL,0);if(r<0)return UINT64_MAX;return (uint64_t)info.free*info.sector*info.per_cluster;}
/* Initialization is resumable: never reinitialize a successful subsystem after
 * a later stage fails. Use the firmware 3.x+ module API on our 6.60 target. */
static int module_loaded[6], init_stage;
static int initialize_network(void) {
    const int modules[] = {PSP_MODULE_NET_COMMON, PSP_MODULE_NET_INET,
        PSP_MODULE_NET_PARSEURI, PSP_MODULE_NET_PARSEHTTP,
        PSP_MODULE_NET_HTTP, PSP_MODULE_NET_SSL};
    const char *names[] = {"COMMON", "INET", "PARSEURI", "PARSEHTTP", "HTTP", "SSL"};
    logmsg("Flow Go 0.7 net init (heap reserve=8192 KiB): max_free=%u total_free=%u stage=%d",
        sceKernelMaxFreeMemSize(), sceKernelTotalFreeMemSize(), init_stage);
    for (int i=0; i<6; ++i) {
        if (module_loaded[i]) continue;
        int r=sceUtilityLoadModule(modules[i]);
        logmsg("LoadModule %s (0x%04X): 0x%08X; free=%u",
            names[i],modules[i],(unsigned)r,sceKernelTotalFreeMemSize());
        if (r<0 && r!=(int)SCE_ERROR_MODULE_ALREADY_LOADED) {
            char label[48]; snprintf(label,sizeof label,"Модуль %s",names[i]);
            return check(r,label);
        }
        module_loaded[i]=1;
    }
    while (init_stage<7) {
        int r; const char *label;
        switch(init_stage) {
        case 0: label="sceNetInit"; r=sceNetInit(256*1024,0x30,4096,0x30,4096); break;
        case 1: label="sceNetInetInit"; r=sceNetInetInit(); break;
        case 2: label="sceNetApctlInit"; r=sceNetApctlInit(0x8000,0x30); break;
        case 3: label="sceNetResolverInit"; r=sceNetResolverInit(); break;
        case 4: label="sceSslInit"; r=sceSslInit(256*1024); break;
        case 5: label="sceHttpInit"; r=sceHttpInit(512*1024); break;
        default:
            /* HTTP downloads do not require a successful HTTPS initialization. */
            r=sceHttpsInit(0,0,0,0);
            logmsg("sceHttpsInit: 0x%08X",(unsigned)r);
            if(r>=0)logmsg("LoadDefaultCert: 0x%08X",(unsigned)sceHttpsLoadDefaultCert(0,0));
            init_stage++; continue;
        }
        logmsg("%s: 0x%08X",label,(unsigned)r);
        if(!check(r,label))return 0;
        init_stage++;
    }
    ready=1;
    return 1;
}
#include "wifi_connect.inc"
static uint64_t filesize(const char *path){struct stat s;return stat(path,&s)==0?(uint64_t)s.st_size:0;}
/* HTTP ranges must match offset; an ignored Range restarts safely, never appends. */
int download_file(const Game *g,const char *path,uint64_t limit){failure[0]=0;if(strncmp(g->url,"http://",7)&&strncmp(g->url,"https://",8))return fail("Нужна прямая HTTP/HTTPS ссылка");if(!connect_wifi())return 0;if(!strncmp(g->url,"https://",8)){return tls_fetch_file(g->url,path,limit,failure,sizeof failure);}int tmpl=-1,conn=-1,req=-1,ok=0;FILE *out=NULL;uint64_t offset=filesize(path),total=g->size,expected=0,received=0;if(offset>limit)return fail("Слишком большой временный файл");if(total&&offset>total)return fail("Размер .part больше файла каталога");if(total&&offset==total){transfer_done=offset;transfer_total=total;return 1;}
tmpl=sceHttpCreateTemplate("FlowGo/0.7",PSP_HTTP_VERSION_1_1,1);if(!check(tmpl,"HTTP шаблон"))goto cleanup;sceHttpSetResolveTimeOut(tmpl,10000000);sceHttpSetConnectTimeOut(tmpl,10000000);sceHttpSetRecvTimeOut(tmpl,15000000);sceHttpSetSendTimeOut(tmpl,15000000);sceHttpEnableRedirect(tmpl);conn=sceHttpCreateConnectionWithURL(tmpl,g->url,0);if(!check(conn,"HTTP соединение"))goto cleanup;req=sceHttpCreateRequestWithURL(conn,PSP_HTTP_METHOD_GET,(char*)g->url,0);if(!check(req,"HTTP запрос"))goto cleanup;sceHttpAddExtraHeader(req,"Accept-Encoding","identity",PSP_HTTP_HEADER_OVERWRITE);if(offset){char range[64];snprintf(range,sizeof range,"bytes=%llu-",(unsigned long long)offset);sceHttpAddExtraHeader(req,"Range",range,PSP_HTTP_HEADER_OVERWRITE);}if(!check(sceHttpSendRequest(req,NULL,0),"Отправка HTTP"))goto cleanup;int status=0;if(!check(sceHttpGetStatusCode(req,&status),"HTTP статус"))goto cleanup;logmsg("HTTP status=%d offset=%llu",status,(unsigned long long)offset);unsigned char *raw=NULL;unsigned len=0;char headers[16385]={0};if(sceHttpGetAllHeader(req,&raw,&len)>=0&&len<sizeof headers){memcpy(headers,raw,len);headers[len]=0;}
if(status==206){char range[128];uint64_t range_total;if(!header_value(headers,"Content-Range",range,sizeof range)||!validate_range(range,offset,&range_total,&expected)){fail("Сервер прислал неверный диапазон");goto cleanup;}if(total&&total!=range_total){fail("Файл на сервере изменился");goto cleanup;}total=range_total;}else if(status==200){offset=0;SceULong64 length=0;if(sceHttpGetContentLength(req,&length)>=0){expected=length;if(total&&total!=length){fail("Размер не совпадает с каталогом");goto cleanup;}total=length;}}else{snprintf(failure,sizeof failure,"HTTP %d: сервер отклонил загрузку",status);goto cleanup;}
if(total>limit||offset>limit){fail("Файл превышает допустимый размер");goto cleanup;}uint64_t space=free_space();if(total&&space!=UINT64_MAX&&total-offset+1024*1024>space){fail("Недостаточно свободного места");goto cleanup;}out=fopen(path,offset?"ab":"wb");if(!out){fail("Не удалось создать .part файл");goto cleanup;}lock();transfer_done=offset;transfer_total=total;transfer_received=0;transfer_started=sceKernelGetSystemTimeWide();transfer_phase=0;unlock();uint8_t buffer[32768];for(;;){if(cancel_job||!running){fail("Загрузка приостановлена");goto cleanup;}int n=sceHttpReadData(req,buffer,sizeof buffer);if(n<0){check(n,"Чтение HTTP");goto cleanup;}if(n==0)break;if(offset+received+(uint64_t)n>limit||(total&&offset+received+(uint64_t)n>total)){fail("Сервер прислал лишние данные");goto cleanup;}if(fwrite(buffer,1,n,out)!=(size_t)n){fail("Ошибка записи, проверь память");goto cleanup;}received+=n;lock();transfer_done=offset+received;transfer_received=received;unlock();sceKernelDelayThread(1000);}if((expected&&received!=expected)||(total&&offset+received!=total)||offset+received==0){fail("Загрузка оборвалась; можно продолжить");goto cleanup;}ok=1;
cleanup:if(!ok)logmsg("Download stopped: %s",failure);if(out&&fclose(out)){ok=0;fail("Не удалось сохранить файл");}if(req>=0)sceHttpDeleteRequest(req);if(conn>=0)sceHttpDeleteConnection(conn);if(tmpl>=0)sceHttpDeleteTemplate(tmpl);return ok;}
static int verify(const Game *g,const char *path){FILE *f=fopen(path,"rb");if(!f)return fail("Нет загруженного файла");SHA256_CTX sha;sha256_init(&sha);uint8_t data[32768],digest[32];size_t n;uint64_t count=0;transfer_done=0;transfer_total=filesize(path);while((n=fread(data,1,sizeof data,f))){if(cancel_job||!running){fclose(f);return fail("Проверка приостановлена");}sha256_update(&sha,data,n);count+=n;transfer_done=count;worker_yield();}int bad=ferror(f);fclose(f);if(bad||!count||(g->size&&count!=g->size))return fail("Неверный размер файла");sha256_final(&sha,digest);if(*g->sha){char hex[65];for(int i=0;i<32;i++)sprintf(hex+i*2,"%02x",digest[i]);if(strcasecmp(hex,g->sha))return fail("SHA-256 не совпал; удали задачу и повтори");}return 1;}
#include "catalog_network.inc"
static void fetch_cover(const char *id){
    char final[128],part[140];snprintf(final,sizeof final,"covers/%s.png",id);snprintf(part,sizeof part,"covers/%s.part",id);
    FILE *existing=fopen(final,"rb");if(existing){fclose(existing);return;}
    FILE *map=fopen(!strncmp(id,"HB_",3)?"homebrew-covers.tsv":"covers.tsv","r");if(!map){logmsg("Cover %s: covers.tsv missing",id);lock();snprintf(message,sizeof message,"Нет списка обложек: нужен полный комплект");unlock();return;}
    char row[1200];Game cover={0};snprintf(cover.id,sizeof cover.id,"%s",id);
    while(fgets(row,sizeof row,map)){char *tab=strchr(row,'\t');if(!tab)continue;*tab++=0;tab[strcspn(tab,"\r\n")]=0;if(!strcmp(row,id)&&strlen(tab)<sizeof cover.url){strcpy(cover.url,tab);break;}}
    fclose(map);if(!cover.url[0]){logmsg("Cover %s: no source entry",id);lock();snprintf(message,sizeof message,"Для этой игры обложка пока не найдена");unlock();return;}
    remove(part);transfer_phase=0;
    int ok=0;
    if(!strncmp(cover.url,"https://",8)){
        char *bytes=NULL;size_t size=0;
        if(connect_wifi()&&tls_fetch_memory(cover.url,&bytes,&size,2*1024*1024,failure,sizeof failure)){
            FILE *f=fopen(part,"wb");if(f){ok=fwrite(bytes,1,size,f)==size;if(fclose(f))ok=0;}
        }
        free(bytes);
    }else ok=download_file(&cover,part,2*1024*1024);
    if(ok){unsigned char head[24];FILE *f=fopen(part,"rb");ok=f&&fread(head,1,sizeof head,f)==sizeof head&&!memcmp(head,"\x89PNG\r\n\x1a\n",8);if(f)fclose(f);if(ok)ok=atomic_replace(part,final);}
    remove(part);
    lock();if(ok||cancel_job)cover_revision++;if(!ok&&!cancel_job)snprintf(message,sizeof message,"Обложка недоступна; подробнее в flow.log");unlock();
    logmsg("Cover %s: %s (%s)",id,ok?"cached":"unavailable",ok?"PNG":failure);
}
int network_worker(unsigned int args,void *argp){(void)args;(void)argp;while(running){int selected=-1,do_refresh=0,do_cover=0;char cover_id[48];Game game;lock();source_apply_pending();int next=queue_next();if(next>=0){int i=next;selected=i;game=jobs[i].game;transfer_done=0;transfer_total=game.size;transfer_received=0;transfer_started=0;transfer_phase=3;jobs[i].state=ACTIVE;busy=1;cancel_job=0;active_job=i;delete_active=0;save_jobs();}else if(refresh_requested){refresh_requested=0;do_refresh=1;busy=1;cancel_job=0;}if(!do_refresh&&selected<0&&cover_pending[0]){snprintf(cover_id,sizeof cover_id,"%s",cover_pending);cover_pending[0]=0;do_cover=1;busy=1;cancel_job=0;}unlock();if(do_refresh){refresh();lock();busy=0;source_apply_pending();unlock();continue;}if(do_cover){fetch_cover(cover_id);lock();busy=0;source_apply_pending();unlock();continue;}if(selected<0){sceKernelDelayThread(100000);continue;}logmsg("Queue start: %s kind=%s",game.id,game.kind);char path[128];snprintf(path,sizeof path,"downloads/%s.part",game.id);int ok=download_file(&game,path,0x7fffffffULL);if(ok){transfer_phase=1;ok=verify(&game,path);}if(ok&&cancel_job){ok=0;fail("Загрузка приостановлена");}if(ok&&!cancel_job){transfer_done=0;transfer_total=0;transfer_phase=2;ok=install_game(&game,path);if(!ok)snprintf(failure,sizeof failure,"%s",message);}lock();selected=active_job;jobs[selected].state=ok?DONE:cancel_job?PAUSED:FAILED;snprintf(jobs[selected].error,sizeof jobs[selected].error,"%s",ok?"":failure);if(ok)remove(path);save_jobs();logmsg("Queue finish: %s ok=%d cancelled=%d error=%s",game.id,ok,cancel_job,failure);busy=0;active_job=-1;if(delete_active){delete_active=0;queue_remove(selected);}unlock();}return 0;}
