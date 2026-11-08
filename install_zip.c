#include "flow.h"
#include "vendor/minizip/unzip.h"
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
static int safe_path(const char *s){
    if(!*s||strlen(s)>200||*s=='/'||strchr(s,':')||strchr(s,'\\'))return 0;
    for(const char *p=s;*p;){const char *q=strchr(p,'/');size_t n=q?(size_t)(q-p):strlen(p);if(!n||(n==1&&*p=='.')||(n==2&&p[0]=='.'&&p[1]=='.'))return 0;for(size_t i=0;i<n;i++)if((unsigned char)p[i]<32)return 0;if(!q)break;p=q+1;}
    return 1;
}
static int mkdir_parents(char *path){for(char *p=strchr(path,'/');p;p=strchr(p+1,'/')){*p=0;mkdir(path,0777);*p='/';}return 1;}
/* Only called for the newly created, fixed staging directory; ZIP symlinks forbidden. */
static void remove_stage(const char *path){DIR *d=opendir(path);if(d){struct dirent *e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;char p[512];if(snprintf(p,sizeof p,"%s/%s",path,e->d_name)>=(int)sizeof p)continue;struct stat st;if(!stat(p,&st)&&S_ISDIR(st.st_mode))remove_stage(p);else remove(p);}closedir(d);}rmdir(path);}
int install_homebrew_zip(const Game *g,const char *source){
    message[0]=0; if(!valid_id(g->id))return 0;
    unzFile zip=unzOpen(source);if(!zip){snprintf(message,sizeof message,"Не удалось открыть ZIP");return 0;}
    unz_global_info global;if(unzGetGlobalInfo(zip,&global)!=UNZ_OK||global.number_entry>8192){unzClose(zip);snprintf(message,sizeof message,"Неверный или слишком большой ZIP");return 0;}
    char root[240]={0},entry[256],stage[300],dest[300];int eboots=0,ok=0,created=0;uint64_t total=0,done=0;
    char (*seen)[256]=calloc(global.number_entry?global.number_entry:1,256);if(!seen){unzClose(zip);return 0;}
    int seen_count=0, next_result;
    if(unzGoToFirstFile(zip)!=UNZ_OK)goto cleanup;
    do{unz_file_info info;if(unzGetCurrentFileInfo(zip,&info,entry,sizeof entry,NULL,0,NULL,0)!=UNZ_OK||info.size_filename>=sizeof entry||!safe_path(entry)||(info.flag&1)||((info.external_fa>>16)&0170000)==0120000)goto cleanup;
        if((unsigned)seen_count>=global.number_entry)goto cleanup;
        for(int i=0;i<seen_count;i++)if(!strcasecmp(seen[i],entry))goto cleanup;strcpy(seen[seen_count++],entry);
        const char *base=strrchr(entry,'/');base=base?base+1:entry;
        if(!strcasecmp(base,"EBOOT.PBP")){size_t prefix=base-entry;if(!eboots||prefix<strlen(root)){memcpy(root,entry,prefix);root[prefix]=0;}eboots++;}
        total+=info.uncompressed_size;if(total>1024ULL*1024*1024)goto cleanup;
    }while((next_result=unzGoToNextFile(zip))==UNZ_OK);
    if(next_result!=UNZ_END_OF_LIST_OF_FILE)goto cleanup;
    if((unsigned)seen_count!=global.number_entry)goto cleanup;
    if(!eboots){snprintf(message,sizeof message,"В ZIP нет EBOOT.PBP");goto cleanup;}
    for(int i=0;i<seen_count;i++){const char *base=strrchr(seen[i],'/');base=base?base+1:seen[i];if(!strcasecmp(base,"EBOOT.PBP")&&strncmp(seen[i],root,strlen(root))){snprintf(message,sizeof message,"В ZIP несколько отдельных приложений");goto cleanup;}}
    uint64_t space=free_space();if(space!=UINT64_MAX&&total+1024*1024>space){snprintf(message,sizeof message,"Недостаточно места для распаковки");goto cleanup;}
    snprintf(dest,sizeof dest,"%s/PSP/GAME/%s",device,g->id);snprintf(stage,sizeof stage,"%s/PSP/GAME/.flow-%s",device,g->id);struct stat st;
    if(!stat(dest,&st)||!stat(stage,&st)){snprintf(message,sizeof message,"Папка игры или распаковки уже существует");goto cleanup;}
    if(mkdir(stage,0777)){snprintf(message,sizeof message,"Не удалось создать папку установки");goto cleanup;}created=1;
    transfer_total=total;transfer_done=0;if(unzGoToFirstFile(zip)!=UNZ_OK)goto cleanup;
    do{unz_file_info info;if(unzGetCurrentFileInfo(zip,&info,entry,sizeof entry,NULL,0,NULL,0)!=UNZ_OK)goto cleanup;
        if(strncmp(entry,root,strlen(root)))continue;const char *relative=entry+strlen(root);if(!*relative)continue;
        char path[512];if(snprintf(path,sizeof path,"%s/%s",stage,relative)>=(int)sizeof path)goto cleanup;mkdir_parents(path);
        if(entry[strlen(entry)-1]=='/'){mkdir(path,0777);continue;}
        if(cancel_job||!running)goto cleanup;
        if(unzOpenCurrentFile(zip)!=UNZ_OK)goto cleanup;FILE *out=fopen(path,"wb");if(!out){unzCloseCurrentFile(zip);goto cleanup;}
        unsigned char buffer[32768];int read,write_ok=1;uint64_t file_bytes=0;
        while((read=unzReadCurrentFile(zip,buffer,sizeof buffer))>0){file_bytes+=read;if(file_bytes>info.uncompressed_size||cancel_job||!running||fwrite(buffer,1,read,out)!=(size_t)read){write_ok=0;break;}done+=read;transfer_done=done;worker_yield();}
        if(fclose(out))write_ok=0;int crc=unzCloseCurrentFile(zip);if(read<0||!write_ok||crc!=UNZ_OK||file_bytes!=info.uncompressed_size)goto cleanup;
    }while((next_result=unzGoToNextFile(zip))==UNZ_OK);
    if(next_result!=UNZ_END_OF_LIST_OF_FILE)goto cleanup;
    char boot[340];snprintf(boot,sizeof boot,"%s/EBOOT.PBP",stage);FILE *f=fopen(boot,"rb");unsigned char magic[4];int valid=f&&fread(magic,1,4,f)==4&&!memcmp(magic,"\0PBP",4);if(f)fclose(f);if(!valid)goto cleanup;
    ok=rename(stage,dest)==0;
cleanup:
    unzClose(zip);free(seen);if(!ok&&created)remove_stage(stage);if(!ok&&!*message)snprintf(message,sizeof message,"ZIP не поддерживается или повреждён");return ok;
}
