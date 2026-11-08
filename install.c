#include "flow.h"
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static jmp_buf trap;
static FILE *opened[8];static int opened_count;
static char destination[320],stage[340];static uint64_t input_size;
_Noreturn void install_fail(const char *why){snprintf(message,sizeof message,"%s",why);longjmp(trap,1);}
void checked_read(FILE *f,uint64_t offset,void *out,uint32_t count){if(cancel_job||!running)install_fail("Установка приостановлена");if(offset>input_size||count>input_size-offset||offset>0x7fffffffULL||fseek(f,(long)offset,SEEK_SET)||fread(out,1,count,f)!=count)install_fail("Повреждённый пакет или ошибка чтения");}
void *pkgi_open(const char *path){if(opened_count>=8)install_fail("Слишком много файлов");FILE *f=fopen(path,"rb");if(!f)install_fail("Не удалось открыть пакет");opened[opened_count++]=f;return f;}
void *pkgi_create(const char *ignored){(void)ignored;if(opened_count>=8)install_fail("Слишком много файлов");uint64_t free=free_space();if(free!=UINT64_MAX&&free<2*1024*1024)install_fail("Недостаточно места для установки");FILE *f=fopen(stage,"wb");if(!f)install_fail("Не удалось создать файл игры");opened[opened_count++]=f;return f;}
void pkgi_close(void *ptr){for(int i=0;i<opened_count;i++)if(opened[i]==ptr)opened[i]=NULL;if(fclose(ptr))install_fail("Ошибка закрытия файла игры");}
int pkgi_write(void *ptr,const void *data,uint32_t size){if(cancel_job||!running)install_fail("Установка приостановлена");if(fwrite(data,1,size,ptr)!=size)install_fail("Память заполнена или ошибка записи");worker_yield();return size;}
void *pkgi_malloc(uint32_t size){void *p=malloc(size);if(!p)install_fail("Не хватает памяти");return p;}void pkgi_free(void *p){free(p);}
int64_t pkgi_get_size(const char *path){struct stat st;return stat(path,&st)?-1:st.st_size;}
const char *pkgi_get_storage_device(void){return device;}int pkgi_mkdirs(const char *path){(void)path;return 1;}
void update_install_progress(const char *path,uint64_t offset){(void)path;transfer_done=offset;transfer_total=input_size;if(cancel_job||!running)install_fail("Установка приостановлена");}
static int valid_game_file(const char *path,const char *kind){FILE *f=fopen(path,"rb");if(!f)return 0;unsigned char h[8]={0};size_t n=fread(h,1,8,f);int ok=0;if(n==8){if(!strcmp(kind,"iso")){if(!fseek(f,0x8001,SEEK_SET)&&fread(h,1,5,f)==5&&!memcmp(h,"CD001",5))ok=1;}else if(!strcmp(kind,"cso"))ok=!memcmp(h,"CISO",4);else if(!strcmp(kind,"pbp"))ok=!memcmp(h,"\0PBP",4);}fclose(f);return ok;}
int install_game(const Game *g,const char *source){if(!strcmp(g->kind,"zip"))return install_homebrew_zip(g,source);char dir[300];int pkg=!strcmp(g->kind,"pkg");if(!valid_id(g->id)){snprintf(message,sizeof message,"Неверный ID игры");return 0;}if(!strcmp(g->kind,"pbp")){snprintf(dir,sizeof dir,"%s/PSP/GAME/%s",device,g->id);mkdir(dir,0777);snprintf(destination,sizeof destination,"%s/EBOOT.PBP",dir);}else{snprintf(dir,sizeof dir,"%s/ISO",device);mkdir(dir,0777);snprintf(destination,sizeof destination,"%s/%s.%s",dir,g->id,pkg?"iso":g->kind);}struct stat st;if(!stat(destination,&st)){snprintf(message,sizeof message,"Игра уже существует; перезапись отключена");return 0;}input_size=pkgi_get_size(source);snprintf(stage,sizeof stage,"%s.installing",destination);opened_count=0;memset(opened,0,sizeof opened);int ok=0;if(setjmp(trap)==0){if(pkg){uint64_t free=free_space();if(free!=UINT64_MAX&&free<1900ULL*1024*1024)install_fail("Для распаковки PKG нужно ещё 1.9 ГиБ");ok=convert_psp_pkg_iso(source,0);if(!ok)snprintf(message,sizeof message,"Пакет не поддерживается; см. flow.log");}else{if(!valid_game_file(source,g->kind))install_fail("Файл не похож на ISO/CSO/PBP");if(rename(source,stage))install_fail("Не удалось переместить скачанную игру");ok=1;}}for(int i=0;i<opened_count;i++)if(opened[i]){fclose(opened[i]);opened[i]=NULL;}if(ok&&!valid_game_file(stage,pkg?"iso":g->kind)){snprintf(message,sizeof message,"Проверка установленного файла не прошла");ok=0;}if(ok&&rename(stage,destination)){snprintf(message,sizeof message,"Не удалось завершить установку");ok=0;}if(!ok){if(!pkg)rename(stage,source);else remove(stage);}return ok;}
