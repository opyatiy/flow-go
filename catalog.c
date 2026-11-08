#include "flow.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

static int compare_game(const void *a,const void *b){return strcasecmp(((const Game*)a)->name,((const Game*)b)->name);}
/* Commit only validated entries. An interrupted replace can recover from .bak. */
int save_catalog_cache(const Game *entries,int count){
    if(count<=0||count>MAX_GAMES)return 0;
    char temporary[100];snprintf(temporary,sizeof temporary,"%s.new",catalog_cache);FILE *f=fopen(temporary,"wb");if(!f)return 0;
    int ok=fputs("id\tname\tregion\tkind\tsize\tsha256\turl\n",f)>=0;
    for(int i=0;i<count&&ok;i++){
        const Game *g=&entries[i];
        ok=fprintf(f,"%s\t%s\t%s\t%s\t%llu\t%s\t%s\n",g->id,g->name,g->region,g->kind,(unsigned long long)g->size,g->sha,g->url)>0;
    }
    if(ferror(f))ok=0;
    if(fclose(f))ok=0;
    if(ok)ok=atomic_replace(temporary,catalog_cache);
    if(!ok)remove(temporary);
    return ok;
}
/* Parses a downloaded buffer in place. No catalog files are created. */
int parse_catalog_memory(char *data,size_t size,Game *out,int cap){
    if(!data||!size||memchr(data,0,size))return -2;
    size_t start=0;if(size>=3&&!memcmp(data,"\xef\xbb\xbf",3))start=3;while(start<size&&isspace((unsigned char)data[start]))start++;
    if(start<size&&data[start]=='{')return parse_homebrew(data+start,size-start,out,cap);
    data[size]=0;char *p=data;int mode=0,n=0;
    if(size>=3&&!memcmp(p,"\xef\xbb\xbf",3))p+=3;
    while(*p){
        char *line=p,*end=strchr(p,'\n');if(end){*end=0;p=end+1;}else p+=strlen(p);
        line[strcspn(line,"\r")]=0;if(!*line||*line=='#')continue;
        char *v[16];int fields=1;v[0]=line;
        for(char *t=line;*t;t++)if(*t=='\t'){*t=0;if(fields==16)return -2;v[fields++]=t+1;}
        if(!mode){
            if(fields==11&&!strcmp(v[0],"Title ID")&&!strcmp(v[3],"Name")&&!strcmp(v[4],"PKG direct link")&&!strcmp(v[9],"File Size")&&!strcmp(v[10],"SHA256")){mode=2;continue;}
            if(fields==7&&!strcmp(v[0],"id")){mode=1;continue;}
            mode=1;
        }
        if(fields!=(mode==2?11:7)){if(mode==2)continue;return -2;}
        const char *id=v[0],*name=v[mode==2?3:1],*region=v[mode==2?1:2];
        const char *kind=mode==2?"pkg":v[3],*url=v[mode==2?4:6],*sha=v[mode==2?10:5],*sz=v[mode==2?9:4];
        char *tail;errno=0;uint64_t bytes=strtoull(sz,&tail,10);
        int good=!errno&&*sz&&!*tail&&*sz!='-'&&bytes<=0x7fffffffULL&&valid_id(id)&&*name&&strlen(name)<192&&strlen(region)<16&&strlen(url)<1024&&(!strncmp(url,"http://",7)||!strncmp(url,"https://",8))&&(!strcmp(kind,"pkg")||!strcmp(kind,"iso")||!strcmp(kind,"cso")||!strcmp(kind,"pbp")||!strcmp(kind,"zip"));
        if(mode==2&&(!bytes||strcmp(v[2],"PSP")||strlen(sha)!=64))good=0;
        if(*sha){if(strlen(sha)!=64)good=0;for(const char *h=sha;*h;h++)if(!isxdigit((unsigned char)*h))good=0;}
        if(!good){if(mode==2)continue;return -2;}
        int duplicate=0;for(int i=0;i<n;i++)if(!strcmp(out[i].id,id)){duplicate=1;break;}if(duplicate)continue;
        if(n>=cap)return -3;
        Game *g=&out[n++];memset(g,0,sizeof *g);strcpy(g->id,id);strcpy(g->name,name);strcpy(g->region,region);strcpy(g->kind,kind);strcpy(g->url,url);strcpy(g->sha,sha);g->size=bytes;
    }
    qsort(out,n,sizeof(Game),compare_game);return n;
}
