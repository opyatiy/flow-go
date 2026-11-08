#include "flow.h"
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
Game games[MAX_GAMES];int game_count,catalog_revision,catalog_loading;
Job jobs[MAX_JOBS];int job_count;
char device[8]="disk",catalog_url[1024],message[160],catalog_status[160];
volatile int busy,cancel_job,refresh_requested,active_job=-1;
void clear_cover_request(void){}
void save_config(void){}
void save_jobs(void){}
int main(void){
 FILE *f=fopen("disk/flow-sources.txt","wb");assert(f);fputs("\xef\xbb\xbf# Test\nCustom | https://example.org/custom.tsv\nInvalid | ftp://example.org/file\n",f);fclose(f);
 sources_init();assert(!source_is_homebrew());source_next();assert(!strcmp(catalog_url,"https://example.org/custom.tsv"));
 busy=1;source_section(1);assert(cancel_job&&!source_is_homebrew());source_apply_pending();assert(!source_is_homebrew());
 busy=0;source_apply_pending();assert(source_is_homebrew()&&!cancel_job);
 busy=1;active_job=2;source_section(0);assert(!source_is_homebrew()&&!cancel_job&&active_job==2);assert(!strcmp(catalog_url,"https://example.org/custom.tsv"));
 active_job=-1;source_section(1);source_section(0);busy=0;source_apply_pending();assert(!source_is_homebrew());
 assert(queue_next()==-1);job_count=3;strcpy(jobs[0].game.id,"A");strcpy(jobs[1].game.id,"B");strcpy(jobs[2].game.id,"C");jobs[0].state=DONE;jobs[1].state=WAITING;jobs[2].state=WAITING;assert(queue_next()==1);
 busy=1;catalog_loading=1;cancel_job=0;refresh_requested=0;queue_wake();assert(cancel_job&&refresh_requested);
 queue_toggle(1);assert(jobs[1].state==PAUSED);queue_toggle(1);assert(jobs[1].state==WAITING);
 cancel_job=0;active_job=1;jobs[1].state=ACTIVE;queue_wake();assert(!cancel_job);
 assert(queue_remove(0));assert(active_job==0&&!strcmp(jobs[0].game.id,"B"));
 assert(!queue_remove(0));assert(delete_active&&cancel_job&&job_count==2);
 active_job=-1;assert(queue_remove(0));assert(job_count==1&&!strcmp(jobs[0].game.id,"C"));
 busy=1;assert(queue_remove(0));assert(job_count==0);
 f=fopen("disk/flow-sources.txt","wb");fputs("# removed on PC\n",f);fclose(f);sources_init();assert(!strcmp(source_name(),"PSP Games"));
 puts("Passed: root source file BOM/comments/deletion, sections, queue priority, pause/resume, deletion during other activity, deferred active deletion, stable active index.");
}
