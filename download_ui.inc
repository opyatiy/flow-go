static void download_details(void){
    uint64_t done=transfer_done,total=transfer_total;
    char line[160],amount[64],size[64];
    if(transfer_phase==3){text(27,198,14,WHITE,"Подключение к серверу...",425);text(27,223,11,MUTED,"Ожидаем начало передачи",425);return;}
    double fraction=total?(double)done/total:0;if(fraction>1)fraction=1;
    box(27,190,425,4,WHITE,35);if(total)box(27,190,(int)(425*fraction),4,RGB(99,215,176),240);
    if(transfer_phase!=0){
        snprintf(line,sizeof line,"%s%s",transfer_phase==1?"Проверка файла":"Установка",total?"":"...");
        if(total)snprintf(line,sizeof line,"%s  %.0f%%",transfer_phase==1?"Проверка файла":"Установка",fraction*100);
        text(27,200,14,WHITE,line,425);text(27,224,11,MUTED,"Файл получен. Игра ещё не готова к запуску",425);return;
    }
    double seconds=transfer_started?(sceKernelGetSystemTimeWide()-transfer_started)/1000000.0:0;
    double rate=seconds>0?transfer_received/seconds:0;
    size_label(done,amount,sizeof amount);size_label(total,size,sizeof size);
    if(total)snprintf(line,sizeof line,"%.1f%%   %s / %s",fraction*100,amount,size);
    else snprintf(line,sizeof line,"Получено %s",amount);
    text(27,200,14,WHITE,line,425);
    if(seconds<1||transfer_received==0){text(27,224,11,MUTED,"Измеряем скорость...",425);return;}
    char speed[40],remaining[50];
    if(rate>=1048576)snprintf(speed,sizeof speed,"%.2f МиБ/с",rate/1048576);else snprintf(speed,sizeof speed,"%.0f КиБ/с",rate/1024);
    if(total&&rate>0&&done<=total){unsigned eta=(unsigned)((total-done)/rate);snprintf(remaining,sizeof remaining,"~%u:%02u осталось",eta/60,eta%60);}else snprintf(remaining,sizeof remaining,"Время неизвестно");
    unsigned elapsed=(unsigned)seconds;
    snprintf(line,sizeof line,"%s   %s   Прошло %u:%02u",speed,remaining,elapsed/60,elapsed%60);text(27,224,11,MUTED,line,425);
}
