#ifndef FLOW_H
#define FLOW_H
#include <stdint.h>
#include <stdio.h>
#define MAX_GAMES 6000
#define MAX_JOBS 16
typedef struct {char id[48],name[192],region[16],url[1024],sha[65],kind[8]; uint64_t size;} Game;
typedef struct {Game game; int state; char error[128];} Job;
enum {WAITING,ACTIVE,PAUSED,DONE,FAILED};
extern Game games[MAX_GAMES]; extern int game_count;
extern Job jobs[MAX_JOBS]; extern int job_count;
extern char appdir[256],device[8],catalog_url[1024],message[160];
extern volatile int running,busy,cancel_job,refresh_requested,active_job;
extern volatile uint64_t transfer_done,transfer_total,transfer_started,transfer_received;
extern volatile int transfer_phase;
extern int profile,theme,dim,wallpaper_index;
extern volatile int music_enabled,music_volume,network_connected;
extern char music_status[80];
extern int auto_covers,cover_revision;
int tls_fetch_memory(const char*,char**,size_t*,size_t,char*,size_t);
extern char catalog_cache[80];
const char *source_name(void);void sources_init(void);int load_active_catalog(void);void source_next(void);
int parse_homebrew(char*,size_t,Game*,int);int install_homebrew_zip(const Game*,const char*);
int tls_fetch_file(const char*,const char*,size_t,char*,size_t);
void request_cover(const char*);void clear_cover_request(void);void source_apply_pending(void);
void source_section(int);int source_is_homebrew(void);
void worker_yield(void);int queue_remove(int);void queue_toggle(int);extern int delete_active;
int queue_next(void);void queue_wake(void);extern int worker_ready;
int music_worker(unsigned int,void*);
void lock(void); void unlock(void); void logmsg(const char*,...);
extern int catalog_loading,catalog_revision;
extern char catalog_status[160];
int save_catalog_cache(const Game*,int);
int parse_catalog_memory(char*,size_t,Game*,int);
int parse_catalog(const char*,Game*,int); int valid_id(const char*);
int header_value(const char*,const char*,char*,size_t);
int validate_range(const char*,uint64_t,uint64_t*,uint64_t*);
int atomic_replace(const char*,const char*);
void save_jobs(void); void load_jobs(void); void save_config(void);
void ui_init(void); void ui_frame(void); void ui_input(uint32_t);
void ui_shutdown(void); void refresh_background(void);
int network_worker(unsigned int,void*); int download_file(const Game*,const char*,uint64_t);
uint64_t free_space(void); int install_game(const Game*,const char*);
int convert_psp_pkg_iso(const char*,int);
_Noreturn void install_fail(const char*); void checked_read(FILE*,uint64_t,void*,uint32_t);
#endif
