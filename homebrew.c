#include "flow.h"
#include "vendor/jsmn.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
static int after(jsmntok_t *t,int n,int i){int end=t[i].end;for(i++;i<n&&t[i].start<end;i++);return i;}
static int key(char *s,jsmntok_t *t,int i,const char *k){return t[i].type==JSMN_STRING&&(size_t)(t[i].end-t[i].start)==strlen(k)&&!memcmp(s+t[i].start,k,strlen(k));}
static int field(char *s,jsmntok_t *t,int n,int obj,const char *k){for(int i=obj+1;i<n&&t[i].start<t[obj].end;){int v=i+1;if(v>=n)return -1;if(key(s,t,i,k))return v;i=after(t,n,v);}return -1;}
static int hex4(const char *s){int v=0;for(int i=0;i<4;i++){unsigned char c=s[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;if(n<0)return -1;v=v*16+n;}return v;}
static int strval(char *s,jsmntok_t *t,int i,char *out,size_t cap){
    if(i<0||t[i].type!=JSMN_STRING)return 0;size_t used=0;
    for(int p=t[i].start;p<t[i].end;p++){
        unsigned c=(unsigned char)s[p];
        if(c=='\\'){if(++p>=t[i].end)return 0;c=(unsigned char)s[p];
            if(c=='u'){
                if(p+4>=t[i].end)return 0;int u=hex4(s+p+1);if(u<0)return 0;p+=4;c=u;
                if(c>=0xD800&&c<=0xDBFF){if(p+6>=t[i].end||s[p+1]!='\\'||s[p+2]!='u')return 0;int lo=hex4(s+p+3);if(lo<0xDC00||lo>0xDFFF)return 0;c=0x10000+((c-0xD800)<<10)+(lo-0xDC00);p+=6;}
                else if(c>=0xDC00&&c<=0xDFFF)return 0;
                if(c<32)return 0;
                unsigned char b[4];int n=0;if(c<128)b[n++]=c;else if(c<2048){b[n++]=0xC0|(c>>6);b[n++]=0x80|(c&63);}else if(c<65536){b[n++]=0xE0|(c>>12);b[n++]=0x80|((c>>6)&63);b[n++]=0x80|(c&63);}else{b[n++]=0xF0|(c>>18);b[n++]=0x80|((c>>12)&63);b[n++]=0x80|((c>>6)&63);b[n++]=0x80|(c&63);}if(used+n>=cap)return 0;memcpy(out+used,b,n);used+=n;continue;
            }else if(c!='/'&&c!='"'&&c!='\\')return 0;
        }
        if(c<32||used+1>=cap)return 0;out[used++]=c;
    }out[used]=0;return 1;
}
int parse_homebrew(char *s,size_t size,Game *out,int cap){
    jsmn_parser parser;jsmn_init(&parser);int count=jsmn_parse(&parser,s,size,NULL,0);if(count<=0||count>150000)return -2;
    jsmntok_t *t=calloc(count,sizeof *t);if(!t)return -1;jsmn_init(&parser);int n=jsmn_parse(&parser,s,size,t,count);if(n<=0||t[0].type!=JSMN_OBJECT){free(t);return -2;}
    int apps=field(s,t,n,0,"apps"),used=0;if(apps<0||t[apps].type!=JSMN_ARRAY){free(t);return -2;}
    FILE *map=fopen("homebrew-covers.tmp","wb");
    for(int i=apps+1;i<n&&t[i].start<t[apps].end;i=after(t,n,i)){
        if(t[i].type!=JSMN_OBJECT)continue;Game g={0};char id[160],sizebuf[32];
        if(!strval(s,t,field(s,t,n,i,"id"),id,sizeof id)||!strval(s,t,field(s,t,n,i,"name"),g.name,sizeof g.name))continue;
        int valid=1;for(char *p=id;*p;p++)if(!isalnum((unsigned char)*p)&&*p!='_'&&*p!='-')valid=0;if(!valid)continue;
        int extra=field(s,t,n,i,"requires_additional_files");if(extra>=0&&t[extra].end-t[extra].start==4&&!memcmp(s+t[extra].start,"true",4)&&strlen(g.name)+8<sizeof g.name)strcat(g.name," [DATA]");
        int releases=field(s,t,n,i,"releases");if(releases<0||t[releases].type!=JSMN_ARRAY||!t[releases].size)continue;int release=releases+1;
        if(!strval(s,t,field(s,t,n,release,"url"),g.url,sizeof g.url)||!strval(s,t,field(s,t,n,release,"sha256"),g.sha,sizeof g.sha))continue;
        int z=field(s,t,n,release,"size");if(z<0||t[z].end-t[z].start>=31)continue;memcpy(sizebuf,s+t[z].start,t[z].end-t[z].start);sizebuf[t[z].end-t[z].start]=0;char *end;g.size=strtoull(sizebuf,&end,10);if(*end||!g.size||g.size>0x7fffffffULL||strlen(g.sha)!=64)continue;
        valid=1;for(char *p=g.sha;*p;p++)if(!isxdigit((unsigned char)*p))valid=0;if(!valid)continue;
        if(strncmp(g.url,"http://",7)&&strncmp(g.url,"https://",8))continue;
        size_t len=strcspn(g.url,"?#");if(len>=4&&!strncasecmp(g.url+len-4,".zip",4))strcpy(g.kind,"zip");else if(len>=4&&!strncasecmp(g.url+len-4,".pbp",4))strcpy(g.kind,"pbp");else continue;
        unsigned h=2166136261u;for(char *p=id;*p;p++)h=(h^(unsigned char)*p)*16777619u;snprintf(g.id,sizeof g.id,"HB_%08X",h);strcpy(g.region,"Homebrew");
        int duplicate=0;for(int j=0;j<used;j++)if(!strcmp(out[j].id,g.id))duplicate=1;if(duplicate)continue;
        if(used>=cap){used=-3;break;}out[used++]=g;
        if(map)fprintf(map,"%s\thttps://pspdev.github.io/homebrew/icons/%s.png\n",g.id,id);
    }
    if(map){int bad=ferror(map);if(fclose(map))bad=1;if(!bad&&used>0)atomic_replace("homebrew-covers.tmp","homebrew-covers.tsv");else remove("homebrew-covers.tmp");}
    free(t);return used;
}
