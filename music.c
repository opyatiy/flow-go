#include "flow.h"
#include <pspkernel.h>
#include <pspaudio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
volatile int music_enabled=1,music_volume=25;
char music_status[80]="";
static uint16_t le16(const unsigned char *p){return p[0]|p[1]<<8;}
static uint32_t le32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
int music_worker(unsigned int args,void *argp){
    (void)args;(void)argp;int channel=-1;FILE *f=NULL;uint32_t bytes=0,left=0;
    short buffer[1024*2] __attribute__((aligned(64)));
    while(running){
        if(!music_enabled){if(f){fclose(f);f=NULL;}if(channel>=0){sceAudioChRelease(channel);channel=-1;}sceKernelDelayThread(100000);continue;}
        if(!f){
            f=fopen("music.wav","rb");unsigned char h[44];
            if(!f){snprintf(music_status,sizeof music_status,"Нет music.wav");sceKernelDelayThread(1000000);continue;}
            if(fread(h,1,44,f)!=44||memcmp(h,"RIFF",4)||memcmp(h+8,"WAVEfmt ",8)||le32(h+16)!=16||le16(h+20)!=1||le16(h+22)!=2||le32(h+24)!=44100||le16(h+34)!=16||memcmp(h+36,"data",4)||!(bytes=le32(h+40))||bytes%4){
                fclose(f);f=NULL;snprintf(music_status,sizeof music_status,"Нужен PCM WAV 44.1k stereo 16-bit");music_enabled=0;continue;
            }
            if(channel<0)channel=sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,1024,PSP_AUDIO_FORMAT_STEREO);
            if(channel<0){fclose(f);f=NULL;snprintf(music_status,sizeof music_status,"Аудиоканал недоступен");music_enabled=0;continue;}
            left=bytes;music_status[0]=0;
        }
        unsigned got=0;memset(buffer,0,sizeof buffer);
        while(got<sizeof buffer){
            unsigned need=sizeof buffer-got;if(need>left)need=left;
            size_t n=fread((unsigned char*)buffer+got,1,need,f);
            if(n!=need){snprintf(music_status,sizeof music_status,"Ошибка чтения music.wav");music_enabled=0;break;}
            got+=n;left-=n;if(!left){if(fseek(f,44,SEEK_SET)){music_enabled=0;break;}left=bytes;}
        }
        int volume=music_volume;if(volume<0)volume=0;if(volume>100)volume=100;
        int result=sceAudioOutputBlocking(channel,PSP_AUDIO_VOLUME_MAX*volume/100,buffer);
        if(result<0){snprintf(music_status,sizeof music_status,"Ошибка аудиовыхода");music_enabled=0;}
    }
    if(f)fclose(f);
    if(channel>=0)sceAudioChRelease(channel);
    return 0;
}
