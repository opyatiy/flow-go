/* Download and validate a new catalog before replacing the persistent cache. */
static void refresh(void){
    const size_t limit=4*1024*1024;
    int tmpl=-1,conn=-1,req=-1,ok=0,count=0;size_t used=0,capacity=65536;
    char *data=NULL;Game *next=NULL;
    failure[0]=0;transfer_phase=0;transfer_done=0;transfer_total=0;
    lock();catalog_loading=1;snprintf(catalog_status,sizeof catalog_status,"Подключение к источнику...");snprintf(message,sizeof message,"Обновление каталога...");unlock();
    if(!connect_wifi())goto cleanup;
    if(!strncmp(catalog_url,"https://",8)){if(!tls_fetch_memory(catalog_url,&data,&used,limit,failure,sizeof failure))goto cleanup;goto parse_download;}
    tmpl=sceHttpCreateTemplate("FlowGo/0.7",PSP_HTTP_VERSION_1_1,1);
    if(!check(tmpl,"Каталог: HTTP шаблон"))goto cleanup;
    sceHttpSetResolveTimeOut(tmpl,10000000);sceHttpSetConnectTimeOut(tmpl,10000000);
    sceHttpSetRecvTimeOut(tmpl,15000000);sceHttpSetSendTimeOut(tmpl,15000000);sceHttpEnableRedirect(tmpl);
    conn=sceHttpCreateConnectionWithURL(tmpl,catalog_url,0);if(!check(conn,"Каталог: соединение"))goto cleanup;
    req=sceHttpCreateRequestWithURL(conn,PSP_HTTP_METHOD_GET,catalog_url,0);if(!check(req,"Каталог: запрос"))goto cleanup;
    sceHttpAddExtraHeader(req,"Accept-Encoding","identity",PSP_HTTP_HEADER_OVERWRITE);
    if(!check(sceHttpSendRequest(req,NULL,0),"Каталог: HTTP/TLS"))goto cleanup;
    int status=0;if(!check(sceHttpGetStatusCode(req,&status),"Каталог: статус"))goto cleanup;
    if(status!=200){snprintf(failure,sizeof failure,"Каталог: HTTP %d",status);goto cleanup;}
    SceULong64 expected=0;int known=sceHttpGetContentLength(req,&expected)>=0;
    if(known&&expected>limit){fail("Каталог превышает 4 МиБ");goto cleanup;}
    transfer_total=known?expected:0;
    data=malloc(capacity+1);if(!data){fail("Нет памяти для каталога");goto cleanup;}
    unsigned char chunk[16384];
    for(;;){
        if(!running||cancel_job){fail("Каталог: загрузка отменена");goto cleanup;}
        int n=sceHttpReadData(req,chunk,sizeof chunk);if(!check(n,"Каталог: чтение"))goto cleanup;if(!n)break;
        if(used+(size_t)n>limit||(known&&used+(size_t)n>expected)){fail("Каталог: лишние данные");goto cleanup;}
        if(used+n>capacity){size_t grown=capacity*2;while(grown<used+n)grown*=2;char *tmp=realloc(data,grown+1);if(!tmp){fail("Нет памяти для каталога");goto cleanup;}data=tmp;capacity=grown;}
        memcpy(data+used,chunk,n);used+=n;transfer_done=used;
    }
    if(!used||(known&&used!=expected)){fail("Каталог загружен не полностью");goto cleanup;}
parse_download:
    next=calloc(MAX_GAMES,sizeof(Game));if(!next){fail("Нет памяти для списка игр");goto cleanup;}
    count=parse_catalog_memory(data,used,next,MAX_GAMES);
    if(count<=0){fail("Источник вернул неверный каталог TSV/JSON");goto cleanup;}
    if(!save_catalog_cache(next,count)){fail("Не удалось сохранить каталог");goto cleanup;}
    lock();memcpy(games,next,count*sizeof(Game));game_count=count;catalog_revision++;unlock();ok=1;
cleanup:
    free(next);free(data);
    if(req>=0)sceHttpDeleteRequest(req);if(conn>=0)sceHttpDeleteConnection(conn);if(tmpl>=0)sceHttpDeleteTemplate(tmpl);
    lock();catalog_loading=0;
    if(ok){snprintf(message,sizeof message,"Каталог обновлён: %d игр",count);snprintf(catalog_status,sizeof catalog_status,"Обновлён: %d игр",count);}
    else{snprintf(message,sizeof message,"%s",failure);snprintf(catalog_status,sizeof catalog_status,"%s",game_count?"Обновление не удалось; список сохранён":"Не удалось загрузить. START — повторить");}
    unlock();
    logmsg("Online catalog: %s; games=%d; bytes=%u",ok?"OK":failure,count,(unsigned)used);
}
