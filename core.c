#include "flow.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

int valid_id(const char *s){size_t n=strlen(s);if(!n||n>40)return 0;for(;*s;s++)if(!isalnum((unsigned char)*s)&&*s!='_'&&*s!='-')return 0;return 1;}
static int split(char *s,char **fields,int cap,char separator){int n=0;fields[n++]=s;for(;*s;s++){if(*s==separator){*s=0;if(n>=cap)return -1;fields[n++]=s+1;}}return n;}
static int copy_field(char *d,size_t cap,const char *s){if(strlen(s)>=cap)return 0;strcpy(d,s);return 1;}
/* Normalized UTF-8 TSV: id, name, region, kind, size, sha256, url. */
int parse_catalog(const char *path,Game *out,int cap){FILE *f=fopen(path,"rb");if(!f)return -1;char line[2048];int n=0,line_no=0;while(fgets(line,sizeof(line),f)){line_no++;size_t len=strlen(line);if(len==sizeof(line)-1&&line[len-1]!='\n'){n=-2;break;}line[strcspn(line,"\r\n")]=0;char *p=line;if(line_no==1&&!memcmp(p,"\xef\xbb\xbf",3))p+=3;if(!*p||*p=='#')continue;char *v[7];int count=split(p,v,7,'\t');if(count!=7){n=-2;break;}if(!strcmp(v[0],"id"))continue;if(n>=cap){n=-3;break;}Game g={0};char *end;errno=0;g.size=strtoull(v[4],&end,10);if(errno||!*v[4]||*end||*v[4]=='-'||g.size>0x7fffffffULL||!valid_id(v[0])||!*v[1]||(!strcmp(v[3],"iso")?0:!strcmp(v[3],"cso")?0:!strcmp(v[3],"pbp")?0:!strcmp(v[3],"zip")?0:!strcmp(v[3],"pkg")?0:1)||(strncmp(v[6],"http://",7)&&strncmp(v[6],"https://",8))){n=-2;break;}if(*v[5]){if(strlen(v[5])!=64){n=-2;break;}int good=1;for(char *h=v[5];*h;h++)if(!isxdigit((unsigned char)*h))good=0;if(!good){n=-2;break;}}if(!copy_field(g.id,sizeof g.id,v[0])||!copy_field(g.name,sizeof g.name,v[1])||!copy_field(g.region,sizeof g.region,v[2])||!copy_field(g.kind,sizeof g.kind,v[3])||!copy_field(g.sha,sizeof g.sha,v[5])||!copy_field(g.url,sizeof g.url,v[6])){n=-2;break;}int dup=0;for(int j=0;j<n;j++)if(!strcmp(out[j].id,g.id)){dup=1;break;}if(!dup)out[n++]=g;}if(ferror(f))n=-1;fclose(f);return n;}
int header_value(const char *headers,const char *name,char *out,size_t cap){size_t k=strlen(name);const char *p=headers;while(*p){const char *end=strchr(p,'\n');if(!end)end=p+strlen(p);if((size_t)(end-p)>k&&p[k]==':'&&!strncasecmp(p,name,k)){const char *start=p+k+1;while(start<end&&(*start==' '||*start=='\t'))start++;while(end>start&&(end[-1]=='\r'||end[-1]==' '))end--;size_t n=end-start;if(n>=cap)return 0;memcpy(out,start,n);out[n]=0;return 1;}if(!*end)break;p=end+1;}return 0;}
int validate_range(const char *value,uint64_t offset,uint64_t *total,uint64_t *length){unsigned long long start,end,size;char trailing;if(sscanf(value,"bytes %llu-%llu/%llu%c",&start,&end,&size,&trailing)!=3||start!=offset||end<start||end>=size||size>0x7fffffffULL)return 0;*total=size;*length=end-start+1;return 1;}
/* Recoverable replace: interrupted replacement leaves an explicit .bak. */
int atomic_replace(const char *tmp,const char *dest){char bak[512];snprintf(bak,sizeof bak,"%s.bak",dest);FILE *f=fopen(dest,"rb");int exists=!!f;if(f)fclose(f);if(exists){remove(bak);if(rename(dest,bak))return 0;}if(rename(tmp,dest)){if(exists)rename(bak,dest);return 0;}if(exists)remove(bak);return 1;}
