#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stnc_config.h"
#include "stnc_platform.h"

#define STNC_DEFAULT_PEER "chain01.stn-chain.org"
#define STNC_DEFAULT_PORT 18473
#define STNC_DEFAULT_ROOT_PEER "chain01.stn-chain.org"
#define STNC_DEFAULT_ROOT_PEER_PORT 18474
#define STNC_DEFAULT_MINING_ENABLED 0
#define STNC_DEFAULT_MINING_BACKEND "automatic"
#define STNC_DEFAULT_MINING_CPU_LIMIT_PERCENT 2u
#define STNC_DEFAULT_STRATUM_HOST "stratum.stn-chain.org"
#define STNC_DEFAULT_STRATUM_PORT 18475

#define STNC_CONFIG_PATH_MAX 1024
#define STNC_CONFIG_FILE_MAX 4096

static stnc_config active_config;
static int config_initialized = 0;

static int stnc_config_set_defaults(void)
{
    size_t peer_length;

    memset(&active_config, 0, sizeof(active_config));

    peer_length = strlen(STNC_DEFAULT_PEER);

    if (peer_length >= sizeof(active_config.peer)) {
        return 1;
    }

    memcpy(
        active_config.peer,
        STNC_DEFAULT_PEER,
        peer_length + 1
    );

    active_config.port = STNC_DEFAULT_PORT;

    peer_length = strlen(STNC_DEFAULT_ROOT_PEER);

    if (peer_length >= sizeof(active_config.root_peer)) {
        return 1;
    }

    memcpy(
        active_config.root_peer,
        STNC_DEFAULT_ROOT_PEER,
        peer_length + 1
    );

    active_config.root_peer_port = STNC_DEFAULT_ROOT_PEER_PORT;
    active_config.mining_enabled = STNC_DEFAULT_MINING_ENABLED;
    memcpy(active_config.mining_backend, STNC_DEFAULT_MINING_BACKEND, sizeof(STNC_DEFAULT_MINING_BACKEND));
    active_config.mining_cpu_limit_percent = STNC_DEFAULT_MINING_CPU_LIMIT_PERCENT;
    memcpy(active_config.stratum_host, STNC_DEFAULT_STRATUM_HOST, sizeof(STNC_DEFAULT_STRATUM_HOST));
    active_config.stratum_port = STNC_DEFAULT_STRATUM_PORT;

    return 0;
}

static int stnc_config_build_path(
    char *buffer,
    size_t buffer_size
)
{
    size_t length;

    if (stnc_platform_get_app_directory(
            buffer,
            buffer_size
        ) != 0) {
        return 1;
    }

    length = strlen(buffer);

    if (length + strlen("\\config.json") + 1 > buffer_size) {
        return 1;
    }

    memcpy(
        buffer + length,
        "\\config.json",
        strlen("\\config.json") + 1
    );

    return 0;
}

static int stnc_config_write_defaults(const char *path)
{
    FILE *file;

    file = fopen(path, "wb");

    if (file == NULL) {
        return 1;
    }

    if (fprintf(
            file,
            "{\n"
            "    \"peer\": \"%s\",\n"
            "    \"port\": %u,\n"
            "    \"root_peer\": \"%s\",\n"
            "    \"root_peer_port\": %u,\n"
            "    \"mining_enabled\": false,\n"
            "    \"mining_backend\": \"automatic\",\n"
            "    \"mining_cpu_limit_percent\": 2,\n"
            "    \"stratum_host\": \"%s\",\n"
            "    \"stratum_port\": %u\n"
            "}\n",
            active_config.peer,
            (unsigned int)active_config.port,
            active_config.root_peer,
            (unsigned int)active_config.root_peer_port,
            active_config.stratum_host,
            (unsigned int)active_config.stratum_port
        ) < 0) {
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        return 1;
    }

    return 0;
}

static const char *stnc_config_find_value(
    const char *json,
    const char *name
)
{
    char key[64];
    const char *position;
    size_t name_length;

    name_length = strlen(name);

    if (name_length + 3 > sizeof(key)) {
        return NULL;
    }

    key[0] = '"';

    memcpy(
        key + 1,
        name,
        name_length
    );

    key[name_length + 1] = '"';
    key[name_length + 2] = '\0';

    position = strstr(json, key);

    if (position == NULL) {
        return NULL;
    }

    position += strlen(key);

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        position++;
    }

    if (*position != ':') {
        return NULL;
    }

    position++;

    while (*position != '\0' &&
           isspace((unsigned char)*position)) {
        position++;
    }

    return position;
}

static int stnc_config_validate_document(const char *json)
{
    const char *start;
    const char *end;

    if (json == NULL) {
        return 1;
    }

    start = json;

    while (*start != '\0' &&
           isspace((unsigned char)*start)) {
        start++;
    }

    if (*start != '{') {
        return 1;
    }

    end = json + strlen(json);

    while (end > start &&
           isspace((unsigned char)*(end - 1))) {
        end--;
    }

    if (end <= start || *(end - 1) != '}') {
        return 1;
    }

    return 0;
}

static int stnc_config_parse_named_peer(
    const char *json,
    const char *name,
    char *peer,
    size_t peer_size
)
{
    const char *value;
    const char *end;
    size_t length;

    value = stnc_config_find_value(json, name);

    if (value == NULL || *value != '"') {
        return 1;
    }

    value++;

    end = strchr(value, '"');

    if (end == NULL) {
        return 1;
    }

    length = (size_t)(end - value);

    if (length == 0 || length >= peer_size) {
        return 1;
    }

    memcpy(peer, value, length);
    peer[length] = '\0';

    return 0;
}

static int stnc_config_parse_named_port(
    const char *json,
    const char *name,
    unsigned short *port
)
{
    const char *value;
    char *end;
    unsigned long parsed;

    value = stnc_config_find_value(json, name);

    if (value == NULL ||
        !isdigit((unsigned char)*value)) {
        return 1;
    }

    errno = 0;

    parsed = strtoul(value, &end, 10);

    if (errno != 0 ||
        end == value ||
        parsed == 0 ||
        parsed > 65535) {
        return 1;
    }

    while (*end != '\0' &&
           isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != ',' &&
        *end != '}') {
        return 1;
    }

    *port = (unsigned short)parsed;

    return 0;
}

static int stnc_config_parse_named_bool(const char *json,const char *name,int *result)
{
    const char *value=stnc_config_find_value(json,name);
    if(value==NULL||result==NULL)return 1;
    if(strncmp(value,"true",4u)==0){*result=1;return 0;}
    if(strncmp(value,"false",5u)==0){*result=0;return 0;}
    return 1;
}

static int stnc_config_parse_mining_backend(const char *json,char *backend,size_t capacity)
{
    const char *value=stnc_config_find_value(json,"mining_backend");const char *end;size_t length;
    if(value==NULL||backend==NULL||capacity==0u||*value!='\"')return 1;
    value++;end=strchr(value,'\"');if(end==NULL)return 1;length=(size_t)(end-value);
    if(length==0u||length>=capacity)return 1;
    if(!((length==9u&&memcmp(value,"automatic",9u)==0)||
         (length==3u&&memcmp(value,"cpu",3u)==0)||
         (length==3u&&memcmp(value,"gpu",3u)==0)||
         (length==8u&&memcmp(value,"usb-asic",8u)==0)))return 1;
    memcpy(backend,value,length);backend[length]='\0';return 0;
}

static int stnc_config_parse_cpu_limit(const char *json,unsigned int *limit)
{
    const char *value=stnc_config_find_value(json,"mining_cpu_limit_percent");char *end;unsigned long parsed;
    if(value==NULL||limit==NULL||!isdigit((unsigned char)*value))return 1;
    errno=0;parsed=strtoul(value,&end,10);
    if(errno!=0||end==value||parsed==0u||parsed>2u)return 1;
    while(*end!='\0'&&isspace((unsigned char)*end))end++;
    if(*end!=','&&*end!='}')return 1;
    *limit=(unsigned int)parsed;return 0;
}

static int stnc_config_load(const char *path)
{
    FILE *file;
    char json[STNC_CONFIG_FILE_MAX];
    size_t length;

    file = fopen(path, "rb");

    if (file == NULL) {
        return 1;
    }

    length = fread(
        json,
        1,
        sizeof(json) - 1,
        file
    );

    if (ferror(file)) {
        fclose(file);
        return 1;
    }

    if (!feof(file)) {
        fclose(file);
        return 1;
    }

    fclose(file);

    json[length] = '\0';

    /*
     * Validate the complete document before extracting
     * individual configuration values.
     */
    if (stnc_config_validate_document(json) != 0) {
        return 1;
    }

    if (stnc_config_parse_named_peer(
            json,
            "peer",
            active_config.peer,
            sizeof(active_config.peer)
        ) != 0) {
        return 1;
    }

    if (stnc_config_parse_named_port(
            json,
            "port",
            &active_config.port
        ) != 0) {
        return 1;
    }

    if (stnc_config_find_value(json, "root_peer") == NULL &&
        stnc_config_find_value(json, "root_peer_port") == NULL) {
        size_t root_peer_length;

        root_peer_length = strlen(STNC_DEFAULT_ROOT_PEER);

        if (root_peer_length >= sizeof(active_config.root_peer)) {
            return 1;
        }

        memcpy(
            active_config.root_peer,
            STNC_DEFAULT_ROOT_PEER,
            root_peer_length + 1
        );

        active_config.root_peer_port = STNC_DEFAULT_ROOT_PEER_PORT;
    } else {
        if (stnc_config_parse_named_peer(
                json,
                "root_peer",
                active_config.root_peer,
                sizeof(active_config.root_peer)
            ) != 0) {
            return 1;
        }

        if (stnc_config_parse_named_port(
                json,
                "root_peer_port",
                &active_config.root_peer_port
            ) != 0) {
            return 1;
        }
    }

    if(stnc_config_find_value(json,"mining_enabled")==NULL&&
       stnc_config_find_value(json,"mining_backend")==NULL&&
       stnc_config_find_value(json,"mining_cpu_limit_percent")==NULL){
        active_config.mining_enabled=STNC_DEFAULT_MINING_ENABLED;
        memcpy(active_config.mining_backend,STNC_DEFAULT_MINING_BACKEND,sizeof(STNC_DEFAULT_MINING_BACKEND));
        active_config.mining_cpu_limit_percent=STNC_DEFAULT_MINING_CPU_LIMIT_PERCENT;
    }else{
        if(stnc_config_parse_named_bool(json,"mining_enabled",&active_config.mining_enabled)!=0||
           stnc_config_parse_mining_backend(json,active_config.mining_backend,sizeof(active_config.mining_backend))!=0||
           stnc_config_parse_cpu_limit(json,&active_config.mining_cpu_limit_percent)!=0)return 1;
    }

    if(stnc_config_find_value(json,"stratum_host")==NULL&&
       stnc_config_find_value(json,"stratum_port")==NULL){
        memcpy(active_config.stratum_host,STNC_DEFAULT_STRATUM_HOST,sizeof(STNC_DEFAULT_STRATUM_HOST));
        active_config.stratum_port=STNC_DEFAULT_STRATUM_PORT;
    }else{
        if(stnc_config_parse_named_peer(json,"stratum_host",active_config.stratum_host,
                sizeof(active_config.stratum_host))!=0||
           stnc_config_parse_named_port(json,"stratum_port",&active_config.stratum_port)!=0)return 1;
    }

    return 0;
}

int stnc_config_init(void)
{
    char path[STNC_CONFIG_PATH_MAX];
    FILE *file;

    if (config_initialized) {
        return 1;
    }

    if (stnc_config_build_path(
            path,
            sizeof(path)
        ) != 0) {
        return 1;
    }

    file = fopen(path, "rb");

    if (file == NULL) {
        if (errno != ENOENT) {
            return 1;
        }

        if (stnc_config_set_defaults() != 0) {
            return 1;
        }

        if (stnc_config_write_defaults(path) != 0) {
            memset(
                &active_config,
                0,
                sizeof(active_config)
            );

            return 1;
        }

        config_initialized = 1;

        return 0;
    }

    fclose(file);

    memset(&active_config, 0, sizeof(active_config));

    if (stnc_config_load(path) != 0) {
        memset(
            &active_config,
            0,
            sizeof(active_config)
        );

        return 1;
    }

    config_initialized = 1;

    return 0;
}

static int stnc_config_write_active(void)
{
    char path[STNC_CONFIG_PATH_MAX],temporary[STNC_CONFIG_PATH_MAX];
    FILE *file;size_t length;
    if(!config_initialized||stnc_config_build_path(path,sizeof(path))!=0)return 1;
    length=strlen(path);if(length+5u>=sizeof(temporary))return 1;
    memcpy(temporary,path,length);memcpy(temporary+length,".tmp",5u);
    file=fopen(temporary,"wb");if(file==NULL)return 1;
    if(fprintf(file,
        "{\n"
        "    \"peer\": \"%s\",\n"
        "    \"port\": %u,\n"
        "    \"root_peer\": \"%s\",\n"
        "    \"root_peer_port\": %u,\n"
        "    \"mining_enabled\": %s,\n"
        "    \"mining_backend\": \"%s\",\n"
        "    \"mining_cpu_limit_percent\": %u,\n"
        "    \"stratum_host\": \"%s\",\n"
        "    \"stratum_port\": %u\n"
        "}\n",
        active_config.peer,(unsigned int)active_config.port,
        active_config.root_peer,(unsigned int)active_config.root_peer_port,
        active_config.mining_enabled?"true":"false",active_config.mining_backend,
        active_config.mining_cpu_limit_percent,active_config.stratum_host,
        (unsigned int)active_config.stratum_port)<0||fclose(file)!=0){
        fclose(file);remove(temporary);return 1;
    }
    if(remove(path)!=0&&errno!=ENOENT){remove(temporary);return 1;}
    if(rename(temporary,path)!=0){remove(temporary);return 1;}
    return 0;
}

static int stnc_config_backend_valid(const char *backend)
{
    return backend!=NULL&&(strcmp(backend,"automatic")==0||strcmp(backend,"cpu")==0||
        strcmp(backend,"gpu")==0||strcmp(backend,"usb-asic")==0);
}

int stnc_config_set_mining_enabled(int enabled)
{
    int previous;if(!config_initialized||(enabled!=0&&enabled!=1))return 1;
    previous=active_config.mining_enabled;active_config.mining_enabled=enabled;
    if(stnc_config_write_active()!=0){active_config.mining_enabled=previous;return 1;}return 0;
}
int stnc_config_set_mining_backend(const char *backend)
{
    char previous[sizeof(active_config.mining_backend)];size_t length;
    if(!config_initialized||!stnc_config_backend_valid(backend))return 1;
    length=strlen(backend);if(length>=sizeof(active_config.mining_backend))return 1;
    memcpy(previous,active_config.mining_backend,sizeof(previous));memset(active_config.mining_backend,0,sizeof(active_config.mining_backend));
    memcpy(active_config.mining_backend,backend,length+1u);
    if(stnc_config_write_active()!=0){memcpy(active_config.mining_backend,previous,sizeof(previous));return 1;}return 0;
}
int stnc_config_set_mining_cpu_limit(unsigned int percent)
{
    unsigned int previous;if(!config_initialized||percent==0u||percent>2u)return 1;
    previous=active_config.mining_cpu_limit_percent;active_config.mining_cpu_limit_percent=percent;
    if(stnc_config_write_active()!=0){active_config.mining_cpu_limit_percent=previous;return 1;}return 0;
}
int stnc_config_reload(void)
{
    char path[STNC_CONFIG_PATH_MAX];stnc_config previous;
    if(!config_initialized||stnc_config_build_path(path,sizeof(path))!=0)return 1;
    previous=active_config;memset(&active_config,0,sizeof(active_config));
    if(stnc_config_load(path)!=0){active_config=previous;return 1;}return 0;
}

const stnc_config *stnc_config_get(void)
{
    if (!config_initialized) {
        return NULL;
    }

    return &active_config;
}

void stnc_config_shutdown(void)
{
    if (!config_initialized) {
        return;
    }

    memset(
        &active_config,
        0,
        sizeof(active_config)
    );

    config_initialized = 0;
}