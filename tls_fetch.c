#include "flow.h"
#include <mbedtls/ssl.h>
#include <psa/crypto.h>
#include <mbedtls/platform_time.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/x509_crt.h>
#include "tilefinch/psp_entropy.h"
#include <pspkernel.h>
#include <psprtc.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

mbedtls_ms_time_t mbedtls_ms_time(void){return (mbedtls_ms_time_t)(sceKernelGetSystemTimeWide()/1000);}

/* Use PSP RTC directly for both the UI check and X.509 validity checks. */
static mbedtls_time_t rtc_time(mbedtls_time_t *out){
    ScePspDateTime epoch={0};epoch.year=1970;epoch.month=1;epoch.day=1;
    u64 now=0,base=0;u32 resolution=sceRtcGetTickResolution();
    mbedtls_time_t value=(mbedtls_time_t)-1;
    if(resolution&&sceRtcGetCurrentTick(&now)>=0&&sceRtcGetTick(&epoch,&base)>=0&&now>=base)
        value=(mbedtls_time_t)((now-base)/resolution);
    if(out)*out=value;return value;
}

typedef struct {
    int fd;unsigned deadline,hard_deadline;
    mbedtls_ssl_context ssl;mbedtls_ssl_config conf;mbedtls_x509_crt ca;
    mbedtls_entropy_context entropy;mbedtls_ctr_drbg_context rng;
    unsigned char readbuf[4096];int pos,length;
} Tls;
static int live(Tls *t){unsigned now=sceKernelGetSystemTimeLow();return running&&!cancel_job&&(int)(now-t->deadline)<0&&(!t->hard_deadline||(int)(now-t->hard_deadline)<0);}
static void deadline(Tls *t){t->deadline=sceKernelGetSystemTimeLow()+30000000u;}
static int transient(void){int e=sceNetInetGetErrno();return e==EAGAIN||e==EWOULDBLOCK||e==EINPROGRESS||e==EINTR;}
static int send_tls(void *ctx,const unsigned char *p,size_t n){Tls *t=ctx;int r=sceNetInetSend(t->fd,p,n,0);if(r>=0)return r;return transient()?MBEDTLS_ERR_SSL_WANT_WRITE:MBEDTLS_ERR_SSL_INTERNAL_ERROR;}
static int recv_tls(void *ctx,unsigned char *p,size_t n){Tls *t=ctx;int r=sceNetInetRecv(t->fd,p,n,0);if(r>=0)return r;return transient()?MBEDTLS_ERR_SSL_WANT_READ:MBEDTLS_ERR_SSL_INTERNAL_ERROR;}
static int read_tls(Tls *t,unsigned char *p,size_t n){
    int r;deadline(t);
    do{if(!live(t))return MBEDTLS_ERR_SSL_TIMEOUT;r=mbedtls_ssl_read(&t->ssl,p,n);if(r==MBEDTLS_ERR_SSL_WANT_READ||r==MBEDTLS_ERR_SSL_WANT_WRITE)sceKernelDelayThread(1000);else return r;}while(1);
}
static int byte_tls(Tls *t){if(t->pos==t->length){int n=read_tls(t,t->readbuf,sizeof t->readbuf);if(n<=0)return -1;t->pos=0;t->length=n;}return t->readbuf[t->pos++];}
static int line_tls(Tls *t,char *line,size_t cap){size_t n=0;int c;while((c=byte_tls(t))>=0){if(c=='\n'){if(n&&line[n-1]=='\r')n--;line[n]=0;return 1;}if(n+1>=cap)return 0;line[n++]=c;}return 0;}
static void entropy_log(const char *s){logmsg("%s",s);}
static void close_tls(Tls *t){if(t->fd>=0)sceNetInetClose(t->fd);mbedtls_ssl_free(&t->ssl);mbedtls_ssl_config_free(&t->conf);mbedtls_x509_crt_free(&t->ca);mbedtls_ctr_drbg_free(&t->rng);mbedtls_entropy_free(&t->entropy);free(t);}
/* Bounded verified HTTPS for catalogs/covers. Game HTTP range path is separate. */
static int tls_fetch(const char *url,char **output,size_t *length,size_t limit,const char *file_path,char *error,size_t error_cap){
    static int entropy_configured;char current[4096];*output=NULL;*length=0;
    if(strlen(url)>=sizeof current){snprintf(error,error_cap,"HTTPS: слишком длинный URL");return 0;}strcpy(current,url);
    if(!entropy_configured){psp_entropy_configure("entropy-seed.bin",PSP_ENTROPY_REQUIRE_CREDIT,entropy_log);entropy_configured=1;}
    mbedtls_platform_set_time(rtc_time);
    ScePspDateTime clock={0};int clock_result=sceRtcGetCurrentClock(&clock,0);
    mbedtls_time_t timestamp=rtc_time(NULL);
    logmsg("PSP RTC: result=0x%08X date=%04u-%02u-%02u %02u:%02u UTC unix=%lld libc=%lld",(unsigned)clock_result,clock.year,clock.month,clock.day,clock.hour,clock.minute,(long long)timestamp,(long long)time(NULL));
    if(clock_result<0||timestamp==(mbedtls_time_t)-1){snprintf(error,error_cap,"Не удалось прочитать часы PSP");return 0;}
    if(clock.year<2020){snprintf(error,error_cap,"Дата PSP %04u-%02u-%02u: установи текущую",clock.year,clock.month,clock.day);return 0;}
    unsigned hard_deadline=file_path?0:sceKernelGetSystemTimeLow()+(limit<=2*1024*1024?30000000u:90000000u);
    for(int redirect=0;redirect<4;redirect++){
        Tls *t=calloc(1,sizeof *t);if(!t){snprintf(error,error_cap,"HTTPS: нет памяти");return 0;}t->fd=-1;t->hard_deadline=hard_deadline;
        mbedtls_ssl_init(&t->ssl);mbedtls_ssl_config_init(&t->conf);mbedtls_x509_crt_init(&t->ca);mbedtls_entropy_init(&t->entropy);mbedtls_ctr_drbg_init(&t->rng);
        int r=-1,ok=0;const char *stage="URL";char host[256],authority[264],path[4096],headers[16384],line[8192],location[4096]={0};char *body=NULL;FILE *sink=NULL;
        if(strncmp(current,"https://",8))goto done;
        const char *slash=strchr(current+8,'/');size_t hostlen=slash?(size_t)(slash-current-8):strlen(current+8);
        if(!hostlen||hostlen>=sizeof host)goto done;
        memcpy(host,current+8,hostlen);host[hostlen]=0;snprintf(path,sizeof path,"%s",slash?slash:"/");
        snprintf(authority,sizeof authority,"%s",host);unsigned port=443;char *colon=strchr(host,':');
        if(colon){*colon++=0;char *end;unsigned long value=strtoul(colon,&end,10);if(!*host||!*colon||*end||value<1||value>65535)goto done;port=(unsigned)value;}
        for(const char *p=host;*p;p++)if((unsigned char)*p<=32||*p==':'||*p=='@'||*p=='#'||*p=='?')goto done;
        for(const char *p=path;*p;p++)if((unsigned char)*p<=32)goto done;
        stage="cacert.pem";FILE *f=fopen("cacert.pem","rb");if(!f)goto done;
        if(fseek(f,0,SEEK_END)){fclose(f);goto done;}long size=ftell(f);rewind(f);
        if(size<=0||size>1024*1024){fclose(f);goto done;}
        unsigned char *pem=malloc(size+1);if(!pem){fclose(f);goto done;}
        size_t got=fread(pem,1,size,f);fclose(f);pem[size]=0;
        r=got==(size_t)size?mbedtls_x509_crt_parse(&t->ca,pem,size+1):-1;free(pem);if(r<0)goto done;
        stage="PSA / случайные данные";r=psa_crypto_init();if(r)goto done;
        stage="случайные данные";r=mbedtls_ctr_drbg_seed(&t->rng,mbedtls_entropy_func,&t->entropy,(const unsigned char*)"FlowGo",6);if(r)goto done;
        stage="TLS настройка";r=mbedtls_ssl_config_defaults(&t->conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);if(r)goto done;
        mbedtls_ssl_conf_authmode(&t->conf,MBEDTLS_SSL_VERIFY_REQUIRED);mbedtls_ssl_conf_ca_chain(&t->conf,&t->ca,NULL);mbedtls_ssl_conf_rng(&t->conf,mbedtls_ctr_drbg_random,&t->rng);
        r=mbedtls_ssl_setup(&t->ssl,&t->conf);if(r)goto done;
        r=mbedtls_ssl_set_hostname(&t->ssl,host);if(r)goto done;
        logmsg("HTTPS stage: DNS host=%s redirect=%d",host,redirect);stage="DNS";int rid=-1;unsigned char dns[1024];struct sockaddr_in addr;memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;addr.sin_port=htons(port);
        r=sceNetResolverCreate(&rid,dns,sizeof dns);if(r<0)goto done;
        r=sceNetResolverStartNtoA(rid,host,&addr.sin_addr,3,2);sceNetResolverDelete(rid);if(r<0)goto done;
        stage="TCP";t->fd=sceNetInetSocket(AF_INET,SOCK_STREAM,0);r=t->fd;if(r<0)goto done;
        int nonblock=1;r=sceNetInetSetsockopt(t->fd,SOL_SOCKET,SO_NONBLOCK,&nonblock,sizeof nonblock);if(r<0)goto done;
        r=sceNetInetConnect(t->fd,(struct sockaddr*)&addr,sizeof addr);
        if(r<0){if(!transient())goto done;deadline(t);int connected=0;while(live(t)){fd_set wr;FD_ZERO(&wr);FD_SET(t->fd,&wr);struct SceNetInetTimeval timeout={0,100000};r=sceNetInetSelect(t->fd+1,NULL,&wr,NULL,&timeout);if(r<0)goto done;if(r>0){int se=0;socklen_t len=sizeof se;r=sceNetInetGetsockopt(t->fd,SOL_SOCKET,SO_ERROR,&se,&len);if(r<0||se){r=se?se:r;goto done;}connected=1;break;}}if(!connected){r=MBEDTLS_ERR_SSL_TIMEOUT;goto done;}}
        mbedtls_ssl_set_bio(&t->ssl,t,send_tls,recv_tls,NULL);stage="TLS handshake";logmsg("HTTPS stage: handshake host=%s",host);deadline(t);
        do{if(!live(t)){r=MBEDTLS_ERR_SSL_TIMEOUT;break;}r=mbedtls_ssl_handshake(&t->ssl);if(r==MBEDTLS_ERR_SSL_WANT_READ||r==MBEDTLS_ERR_SSL_WANT_WRITE)sceKernelDelayThread(1000);else break;}while(1);
        if(r){logmsg("TLS verify flags=0x%08X",(unsigned)mbedtls_ssl_get_verify_result(&t->ssl));goto done;}
        logmsg("HTTPS %s: %s / %s",host,mbedtls_ssl_get_version(&t->ssl),mbedtls_ssl_get_ciphersuite(&t->ssl));
        char request[4608];int requestlen=snprintf(request,sizeof request,"GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: FlowGo/0.7\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",path,authority);if(requestlen<0||requestlen>=(int)sizeof request)goto done;
        stage="HTTPS отправка";deadline(t);for(int sent=0;sent<requestlen;){if(!live(t)){r=MBEDTLS_ERR_SSL_TIMEOUT;goto done;}r=mbedtls_ssl_write(&t->ssl,(unsigned char*)request+sent,requestlen-sent);if(r>0)sent+=r;else if(r==MBEDTLS_ERR_SSL_WANT_WRITE||r==MBEDTLS_ERR_SSL_WANT_READ)sceKernelDelayThread(1000);else goto done;}
        stage="HTTP заголовки";if(!line_tls(t,line,sizeof line)){r=-1;goto done;}int status=0;if(sscanf(line,"HTTP/%*s %d",&status)!=1){r=-1;goto done;}
        logmsg("HTTPS status=%d host=%s",status,host);headers[0]=0;size_t hlen=0;for(;;){if(!line_tls(t,line,sizeof line)){r=-1;goto done;}if(!*line)break;size_t len=strlen(line);if(hlen+len+2>=sizeof headers){r=-1;goto done;}memcpy(headers+hlen,line,len);hlen+=len;headers[hlen++]='\n';headers[hlen]=0;}
        if(status==301||status==302||status==303||status==307||status==308){
            if(!header_value(headers,"Location",location,sizeof location)){r=status;goto done;}
            if(location[0]=='/'&&location[1]!='/')r=snprintf(current,sizeof current,"https://%s%s",authority,location);
            else if(!strncmp(location,"https://",8))r=snprintf(current,sizeof current,"%s",location);else{r=-1;goto done;}
            if(r<0||r>=(int)sizeof current){r=-1;goto done;}close_tls(t);continue;
        }
        stage="HTTP статус";if(status!=200){r=status;goto done;}
        char value[128];int chunked=0,known=0;size_t expected=0;
        stage="HTTP формат";
        if(header_value(headers,"Content-Encoding",value,sizeof value)&&strcasecmp(value,"identity")){r=-1;goto done;}
        if(header_value(headers,"Transfer-Encoding",value,sizeof value)){if(strcasecmp(value,"chunked")){r=-1;goto done;}chunked=1;}
        if(!chunked&&header_value(headers,"Content-Length",value,sizeof value)){char *end;errno=0;unsigned long long n=strtoull(value,&end,10);if(errno||!*value||*end||*value=='-'||n>limit){r=-1;goto done;}expected=n;known=1;}
        if(file_path){sink=fopen(file_path,"wb");if(!sink){stage="Запись файла";r=-1;goto done;}}
        else{body=malloc(limit+1);if(!body){stage="память";r=-1;goto done;}}
        if(sink)setvbuf(sink,NULL,_IOFBF,65536);
        size_t used=0;transfer_total=known?expected:0;transfer_done=0;
        if(file_path){transfer_started=sceKernelGetSystemTimeWide();transfer_received=0;transfer_phase=0;}
        stage="HTTP данные";
        for(;;){
            size_t chunk=0;
            if(chunked){
                if(!line_tls(t,line,sizeof line)){r=-1;goto done;}char *end;errno=0;unsigned long long n=strtoull(line,&end,16);
                if(errno||end==line||(*end&&*end!=';')||n>limit-used){r=-1;goto done;}
                if(!n){size_t trailers=0;do{if(!line_tls(t,line,sizeof line)){r=-1;goto done;}trailers+=strlen(line)+2;if(trailers>16384){r=-1;goto done;}}while(*line);break;}
                chunk=n;
            }else{if(known&&used==expected)break;chunk=known?expected-used:8192;}
            size_t remaining=chunk;
            while(remaining){
                if(cancel_job||!running){r=MBEDTLS_ERR_SSL_TIMEOUT;goto done;}worker_yield();
                unsigned char buffer[8192];size_t want=remaining<sizeof buffer?remaining:sizeof buffer,n;
                if(t->pos<t->length){n=t->length-t->pos;if(n>want)n=want;memcpy(buffer,t->readbuf+t->pos,n);t->pos+=n;}
                else{r=read_tls(t,buffer,want);if(r==0||r==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){if(chunked||known){r=-1;goto done;}goto body_done;}if(r<0)goto done;n=r;}
                if(n>limit-used){r=-1;goto done;}
                if(sink){if(fwrite(buffer,1,n,sink)!=n){stage="Запись файла";r=-1;goto done;}}
                else memcpy(body+used,buffer,n);
                used+=n;remaining-=n;transfer_done=used;if(file_path)transfer_received=used;
            }
            if(chunked&&(byte_tls(t)!='\r'||byte_tls(t)!='\n')){r=-1;goto done;}
        }
body_done:
        if(sink){int closed=fclose(sink);sink=NULL;if(closed){stage="Запись файла";r=-1;goto done;}}
        if(body)body[used]=0;*output=body;*length=used;body=NULL;ok=1;
done:
        if(!ok){snprintf(error,error_cap,"%s: %d",stage,r);logmsg("Modern HTTPS failed (%s), code=%d hex=0x%08X",stage,r,(unsigned)r);}
        if(sink)fclose(sink);free(body);close_tls(t);return ok;
    }
    snprintf(error,error_cap,"HTTPS: слишком много перенаправлений");return 0;
}

int tls_fetch_memory(const char *url,char **output,size_t *length,size_t limit,char *error,size_t cap){return tls_fetch(url,output,length,limit,NULL,error,cap);}
int tls_fetch_file(const char *url,const char *path,size_t limit,char *error,size_t cap){char *unused=NULL;size_t size=0;return tls_fetch(url,&unused,&size,limit,path,error,cap);}
