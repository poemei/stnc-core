#include <stddef.h>

#include "stnc_network.h"
#include "stnc_platform.h"

static int network_initialized = 0;

int stnc_network_init(void)
{
    if (network_initialized) {
        return 1;
    }

    if (stnc_platform_network_init() != 0) {
        return 1;
    }

    network_initialized = 1;

    return 0;
}

void stnc_network_shutdown(void)
{
    if (!network_initialized) {
        return;
    }

    stnc_platform_network_shutdown();

    network_initialized = 0;
}

int stnc_network_connect(
    stnc_network_connection *connection,
    const char *peer,
    unsigned short port
)
{
    if (!network_initialized ||
        connection == NULL ||
        peer == NULL ||
        peer[0] == '\0' ||
        port == 0) {
        return 1;
    }

    connection->handle = NULL;
    connection->connected = 0;

    if (stnc_platform_network_connect(
            &connection->handle,
            peer,
            port
        ) != 0) {
        connection->handle = NULL;
        return 1;
    }

    connection->connected = 1;

    return 0;
}

int stnc_network_send(
    stnc_network_connection *connection,
    const unsigned char *buffer,
    size_t length
)
{
    if (!network_initialized ||
        connection == NULL ||
        !connection->connected ||
        connection->handle == NULL ||
        buffer == NULL ||
        length == 0) {
        return 1;
    }

    return stnc_platform_network_send(
        connection->handle,
        buffer,
        length
    );
}

int stnc_network_receive(
    stnc_network_connection *connection,
    unsigned char *buffer,
    size_t length
)
{
    if (!network_initialized ||
        connection == NULL ||
        !connection->connected ||
        connection->handle == NULL ||
        buffer == NULL ||
        length == 0) {
        return 1;
    }

    return stnc_platform_network_receive(
        connection->handle,
        buffer,
        length
    );
}

void stnc_network_disconnect(
    stnc_network_connection *connection
)
{
    if (connection == NULL ||
        !connection->connected) {
        return;
    }

    stnc_platform_network_disconnect(
        connection->handle
    );

    connection->handle = NULL;
    connection->connected = 0;
}

int stnc_network_is_connected(
    const stnc_network_connection *connection
)
{
    if (connection == NULL) {
        return 0;
    }

    return connection->connected;
}
