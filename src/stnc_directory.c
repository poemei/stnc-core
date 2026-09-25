#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stnc_directory.h"

static const char *find_literal(
    const char *begin,
    const char *end,
    const char *literal
)
{
    size_t length;
    const char *cursor;

    length = strlen(literal);

    for (cursor = begin; cursor + length <= end; ++cursor) {
        if (memcmp(cursor, literal, length) == 0) {
            return cursor;
        }
    }

    return NULL;
}

static const char *skip_space(const char *cursor, const char *end)
{
    while (cursor < end && isspace((unsigned char)*cursor)) {
        ++cursor;
    }

    return cursor;
}

static int read_string(
    const char **cursor,
    const char *end,
    char *buffer,
    size_t capacity
)
{
    size_t length;
    const char *p;

    p = skip_space(*cursor, end);

    if (p >= end || *p != '"' || capacity == 0) {
        return 1;
    }

    ++p;
    length = 0;

    while (p < end && *p != '"') {
        if (*p == '\\' || (unsigned char)*p < 0x20u ||
            length + 1u >= capacity) {
            return 1;
        }

        buffer[length++] = *p++;
    }

    if (p >= end || *p != '"') {
        return 1;
    }

    buffer[length] = '\0';
    *cursor = p + 1;
    return 0;
}

static int read_unsigned(
    const char **cursor,
    const char *end,
    unsigned long *value
)
{
    char number[16];
    size_t length;
    char *tail;
    const char *p;

    p = skip_space(*cursor, end);
    length = 0;

    while (p < end && isdigit((unsigned char)*p)) {
        if (length + 1u >= sizeof(number)) {
            return 1;
        }

        number[length++] = *p++;
    }

    if (length == 0) {
        return 1;
    }

    number[length] = '\0';
    *value = strtoul(number, &tail, 10);

    if (*tail != '\0') {
        return 1;
    }

    *cursor = p;
    return 0;
}

int stnc_directory_decode(
    const char *json,
    size_t length,
    stnc_peer_candidates *candidates
)
{
    const char *end;
    const char *cursor;
    const char *array;
    const char *version;
    const char *network;
    const char *protocol;
    const char *authority;
    const char *count_field;
    unsigned long version_value;
    unsigned long declared_count;
    stnc_peer_candidates staged;
    size_t parsed_count;

    if (json == NULL || candidates == NULL || length == 0 ||
        length > STNC_DIRECTORY_RESPONSE_MAX) {
        return 1;
    }

    end = json + length;

    version = find_literal(json, end, "\"version\":");
    network = find_literal(json, end, "\"network\":\"stn-chain\"");
    protocol = find_literal(json, end, "\"protocol\":\"stnp\"");
    authority = find_literal(json, end, "\"authority\":false");
    count_field = find_literal(json, end, "\"count\":");
    array = find_literal(json, end, "\"peers\":[");

    if (version == NULL || network == NULL || protocol == NULL ||
        authority == NULL || count_field == NULL || array == NULL) {
        return 1;
    }

    cursor = version + strlen("\"version\":");

    if (read_unsigned(&cursor, end, &version_value) != 0 ||
        version_value != 1u) {
        return 1;
    }

    cursor = count_field + strlen("\"count\":");

    if (read_unsigned(&cursor, end, &declared_count) != 0 ||
        declared_count > STNC_PEERS_MAX) {
        return 1;
    }

    staged = *candidates;
    cursor = array + strlen("\"peers\":[");
    parsed_count = 0;

    for (;;) {
        stnc_peer_candidate candidate;
        unsigned long port;

        cursor = skip_space(cursor, end);

        if (cursor >= end) {
            return 1;
        }

        if (*cursor == ']') {
            ++cursor;
            break;
        }

        if (parsed_count != 0) {
            if (*cursor != ',') {
                return 1;
            }
            cursor = skip_space(cursor + 1, end);
        }

        if (cursor >= end || *cursor != '{') {
            return 1;
        }

        ++cursor;
        cursor = skip_space(cursor, end);

        if (cursor + 7 > end || memcmp(cursor, "\"host\":", 7) != 0) {
            return 1;
        }

        cursor += 7;
        memset(&candidate, 0, sizeof(candidate));

        if (read_string(&cursor, end, candidate.host, sizeof(candidate.host)) != 0) {
            return 1;
        }

        cursor = skip_space(cursor, end);

        if (cursor >= end || *cursor != ',') {
            return 1;
        }

        cursor = skip_space(cursor + 1, end);

        if (cursor + 7 > end || memcmp(cursor, "\"port\":", 7) != 0) {
            return 1;
        }

        cursor += 7;

        if (read_unsigned(&cursor, end, &port) != 0 ||
            port == 0 || port > 65535u) {
            return 1;
        }

        candidate.port = (uint16_t)port;
        cursor = skip_space(cursor, end);

        if (cursor >= end || *cursor != '}') {
            return 1;
        }

        ++cursor;

        if (stnc_peers_add(&staged, &candidate) != 0) {
            return 1;
        }

        ++parsed_count;
    }

    if (parsed_count != (size_t)declared_count) {
        return 1;
    }

    *candidates = staged;
    return 0;
}
