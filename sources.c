#include "flow.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
typedef struct {char name[64],url[1024];} Source;
static Source sources[12];static int source_count,source_index;static int pending_source=-1,last_games_source;
char catalog_cache[80]="catalog.tsv";
const char *source_name(void){return sources[source_index].name;}
static unsigned urlhash(const char *s){unsigned h=2166136261u;while(*s)h=(h^(unsigned char)*s++)*16777619u;return h;}
static int valid_source(const char *s){return s&&strlen(s)<1024&&(!strncmp(s,"http://",7)||!strncmp(s,"https://",8))&&!strpbrk(s,"\r\n\t ");}
static void cache_name(void){if(source_index==0)strcpy(catalog_cache,"catalog.tsv");else snprintf(catalog_cache,sizeof catalog_cache,"catalog-%08X.tsv",urlhash(catalog_url));}
static char *trim(char *p){while(*p==' '||*p=='\t')p++;size_t n=strlen(p);while(n&&(p[n-1]==' '||p[n-1]=='\t'||p[n-1]=='\r'||p[n-1]=='\n'))p[--n]=0;return p;}
static void read_sources(FILE *f){char row[1400];int line=0;while(fgets(row,sizeof row,f)&&source_count<12){line++;if(!strchr(row,'\n')&&!feof(f)){int c;while((c=fgetc(f))!=EOF&&c!='\n');continue;}char *p=row;if(line==1&&strlen(p)>=3&&!memcmp(p,"\xef\xbb\xbf",3))p+=3;p=trim(p);if(!*p||*p=='#')continue;char *sep=strchr(p,'|');if(!sep)sep=strchr(p,'\t');if(!sep)continue;*sep++=0;char *name=trim(p),*url=trim(sep);if(!*name||strlen(name)>=64||!valid_source(url))continue;int duplicate=0;for(int i=0;i<source_count;i++)if(!strcmp(url,sources[i].url))duplicate=1;if(duplicate)continue;strcpy(sources[source_count].name,name);strcpy(sources[source_count++].url,url);}}
void sources_init(void){
    strcpy(sources[0].name,"PSP Games");strcpy(sources[0].url,"https://raw.githubusercontent.com/txy7795679/PSVITA-PKGJ-DATADB/master/PSP_GAMES.tsv");
    strcpy(sources[1].name,"PSPDEV Homebrew");strcpy(sources[1].url,"https://pspdev.github.io/homebrew/catalog.json");source_count=2;pending_source=-1;
    char path[80];snprintf(path,sizeof path,"%s/flow-sources.txt",device);
    FILE *f=fopen(path,"rb");int existing=!!f;
    if(f){read_sources(f);fclose(f);}else{f=fopen("sources.tsv","rb");if(f){read_sources(f);fclose(f);}}
    source_index=0;int found=0;for(int i=0;i<source_count;i++)if(!strcmp(catalog_url,sources[i].url)){source_index=i;found=1;break;}
    if(!existing){
        if(!found&&valid_source(catalog_url)&&source_count<12){source_index=source_count++;strcpy(sources[source_index].name,"Мой каталог");strcpy(sources[source_index].url,catalog_url);}
        f=fopen(path,"wb");if(f){fputs("# Flow Go: UTF-8. Name | direct HTTP/HTTPS catalog URL\n# Edit on PC, then restart Flow. Up to 10 custom sources.\n# Example | https://example.org/catalog.tsv\n",f);for(int i=2;i<source_count;i++)fprintf(f,"%s | %s\n",sources[i].name,sources[i].url);fclose(f);}
    }
    strcpy(catalog_url,sources[source_index].url);cache_name();last_games_source=source_index==1?0:source_index;
}
int load_active_catalog(void){char bak[96];snprintf(bak,sizeof bak,"%s.bak",catalog_cache);int n=parse_catalog(catalog_cache,games,MAX_GAMES);if(n<=0)n=parse_catalog(bak,games,MAX_GAMES);return n>0?n:0;}
/* Called with the UI/worker mutex held. Never replace a catalog while
 * refresh() is still writing its cache or publishing its parsed entries. */
static void select_source(int index){
    source_index=index;if(index!=1)last_games_source=index;strcpy(catalog_url,sources[source_index].url);cache_name();
    clear_cover_request();game_count=load_active_catalog();catalog_revision++;
    refresh_requested=game_count==0;
    snprintf(catalog_status,sizeof catalog_status,"%s: %d игр",source_name(),game_count);
    snprintf(message,sizeof message,"Источник: %s",source_name());save_config();
}
static void request_source(int index){
    if(busy&&active_job<0){
        pending_source=index;cancel_job=1;clear_cover_request();
        snprintf(message,sizeof message,"Переключение: %s",sources[index].name);
        return;
    }
    pending_source=-1;select_source(index);
}
int source_is_homebrew(void){return source_index==1;}
void source_section(int homebrew){int target=homebrew?1:last_games_source;if(pending_source>=0||source_index!=target)request_source(target);}
void source_apply_pending(void){
    if(pending_source>=0&&!busy){int index=pending_source;pending_source=-1;cancel_job=0;select_source(index);}
}
void source_next(void){int index=pending_source>=0?pending_source:last_games_source;do{index=(index+1)%source_count;}while(index==1);request_source(index);}
