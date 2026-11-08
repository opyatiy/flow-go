#include <string.h>
#include <errno.h>
#include "flow.h"
/* UI and worker call these with the shared mutex held. */
int queue_next(void){for(int i=0;i<job_count;i++)if(jobs[i].state==WAITING)return i;return -1;}
void queue_wake(void){
    clear_cover_request();
    if(busy&&active_job<0){if(catalog_loading)refresh_requested=1;cancel_job=1;}
}

int delete_active;
void queue_toggle(int i){
    if(i<0||i>=job_count)return;
    Job *j=&jobs[i];
    if(i==active_job){cancel_job=1;snprintf(message,sizeof message,"Останавливаем текущую операцию...");}
    else if(j->state==WAITING)j->state=PAUSED;
    else if(j->state==PAUSED||j->state==FAILED){j->state=WAITING;queue_wake();}
    save_jobs();
}
int queue_remove(int i){
    if(i<0||i>=job_count)return 0;
    if(i==active_job){delete_active=1;cancel_job=1;snprintf(message,sizeof message,"Остановка и удаление задачи...");return 0;}
    char path[128];snprintf(path,sizeof path,"downloads/%s.part",jobs[i].game.id);
    if(remove(path)&&errno!=ENOENT){snprintf(message,sizeof message,"Не удалось удалить временный файл");return 0;}
    memmove(jobs+i,jobs+i+1,(job_count-i-1)*sizeof(Job));job_count--;
    if(active_job>i)active_job--;
    save_jobs();return 1;
}
