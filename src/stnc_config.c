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
            "    \"root_peer_port\": %u\n"
            "}\n",
            active_config.peer,
            (unsigned int)active_config.port,
            active_config.root_peer,
            (unsigned int)active_config.root_peer_port
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