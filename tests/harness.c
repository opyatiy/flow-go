
#include "flow.h"
#include <stdlib.h>
#include <string.h>
char device[8]="disk",message[160],catalog_cache[80]="cache.tsv";
volatile int running=1,cancel_job=0;
volatile uint64_t transfer_done,transfer_total,transfer_started,transfer_received;
int main(int argc,char **argv){
 if(argc!=3)return 4;
 if(!strcmp(argv[1],"zip")){Game g={0};strcpy(g.id,"TEST");int ok=install_homebrew_zip(&g,argv[2]);printf("zip=%d %s\n",ok,message);return ok?0:1;}
 FILE *f=fopen(argv[2],"rb");if(!f)return 5;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *s=calloc(n+1,1);fread(s,1,n,f);fclose(f);Game *g=calloc(MAX_GAMES,sizeof(Game));int count=parse_catalog_memory(s,n,g,MAX_GAMES);printf("count=%d\n",count);if(count>0){for(int i=0;i<count;i++)if(strstr(g[i].name,"\xe6\x97\xa5"))printf("unicode=%s\n",g[i].name);if(!save_catalog_cache(g,count))return 6;Game *copy=calloc(MAX_GAMES,sizeof(Game));int round=parse_catalog("cache.tsv",copy,MAX_GAMES);if(round!=count||memcmp(g,copy,count*sizeof(Game)))return 7;free(copy);}free(s);free(g);return count>0?0:1;
}
uint64_t free_space(void){return UINT64_MAX;}

void worker_yield(void){}
