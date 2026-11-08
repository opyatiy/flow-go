static volatile int wifi_error,wifi_error_seen;
static int wifi_handler=-1,connected_profile=-1;
static void wifi_event(int old_state,int new_state,int event,int error,void *arg){
    (void)old_state;(void)arg;
    network_connected=new_state==PSP_NET_APCTL_STATE_GOT_IP;
    if(event==PSP_NET_APCTL_EVENT_ERROR||error){wifi_error=error;wifi_error_seen=1;}
}
static const char *wifi_stage(int state){
    switch(state){
    case 0:return "нет соединения";
    case 1:return "поиск точки";
    case 2:return "соединение с точкой";
    case 3:return "получение IP";
    case 4:return "подключено";
    case 5:return "авторизация";
    case 6:return "обмен ключами";
    default:return "неизвестный этап";
    }
}
static int connect_wifi(void){
    if(!ready&&!initialize_network())return 0;
    int state=0,r=sceNetApctlGetState(&state);
    if(!check(r,"Wi-Fi: состояние"))return 0;
    if(state==PSP_NET_APCTL_STATE_GOT_IP&&(connected_profile<0||connected_profile==profile)){network_connected=1;return 1;}
    network_connected=0;
    if(sceWlanGetSwitchState()<=0)return fail("Включи WLAN на PSP");
    if(sceUtilityCheckNetParam(profile)<0)return fail("Выбери Wi-Fi профиль в настройках Flow");
    if(wifi_handler<0){wifi_handler=sceNetApctlAddHandler(wifi_event,NULL);if(!check(wifi_handler,"Wi-Fi: обработчик"))return 0;}
    netData name;memset(&name,0,sizeof name);
    r=sceUtilityGetNetParam(profile,PSP_NETPARAM_NAME,&name);
    logmsg("Wi-Fi attempt: profile=%d name=%.64s state=%d",profile,r>=0?name.asString:"?",state);
    /* Disconnect is asynchronous. A fixed 200 ms delay was not sufficient. */
    if(state!=PSP_NET_APCTL_STATE_DISCONNECTED){
        if(!check(sceNetApctlDisconnect(),"Wi-Fi: отключение"))return 0;
        int waited;
        for(waited=0;waited<50&&running&&!cancel_job;waited++){
            if(!check(sceNetApctlGetState(&state),"Wi-Fi: ожидание"))return 0;
            if(state==PSP_NET_APCTL_STATE_DISCONNECTED)break;
            sceKernelDelayThread(100000);
        }
        if(state!=PSP_NET_APCTL_STATE_DISCONNECTED)return fail("Wi-Fi: предыдущее соединение занято");
    }
    if(!running||cancel_job)return fail("Подключение отменено");
    wifi_error=0;wifi_error_seen=0;connected_profile=-1;
    if(!check(sceNetApctlConnect(profile),"Wi-Fi: запуск"))return 0;
    int previous=-1;
    for(int i=0;i<450&&running&&!cancel_job;i++){
        if(!check(sceNetApctlGetState(&state),"Wi-Fi: состояние"))return 0;
        if(state!=previous){
            logmsg("Wi-Fi state=%d (%s)",state,wifi_stage(state));previous=state;
            lock();snprintf(message,sizeof message,"Wi-Fi #%d: %s",profile,wifi_stage(state));unlock();
        }
        if(wifi_error_seen){
            snprintf(failure,sizeof failure,"Wi-Fi #%d: 0x%08X (этап %d)",profile,(unsigned)wifi_error,state);
            logmsg("%s",failure);sceNetApctlDisconnect();return 0;
        }
        if(state==PSP_NET_APCTL_STATE_GOT_IP){network_connected=1;connected_profile=profile;logmsg("Wi-Fi connected: profile=%d",profile);return 1;}
        if(sceWlanGetSwitchState()<=0){sceNetApctlDisconnect();return fail("WLAN выключен во время подключения");}
        sceKernelDelayThread(100000);
    }
    sceNetApctlDisconnect();
    if(!running||cancel_job)return fail("Подключение отменено");
    snprintf(failure,sizeof failure,"Wi-Fi #%d: таймаут — %s",profile,wifi_stage(state));logmsg("%s",failure);return 0;
}
