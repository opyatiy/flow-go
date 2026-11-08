#include "flow.h"
#include <pspkernel.h>
/* Network/installation thread only. Bound uninterrupted CPU work. */
void worker_yield(void){static unsigned last;unsigned now=sceKernelGetSystemTimeLow();if(now-last>=8000){sceKernelDelayThread(1000);last=sceKernelGetSystemTimeLow();}}
