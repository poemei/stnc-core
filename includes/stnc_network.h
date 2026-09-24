#ifndef STNC_NETWORK_H
#define STNC_NETWORK_H

#include <stddef.h>

typedef struct stnc_network_connection {
    void *handle;
    int connected;
} stnc_network_connection;

int stnc_network_init(void);
void stnc_network_shutdown(void);

int stnc_network_connect(
    stnc_network_connection *connection,
    const char *peer,
    unsigned short port
);

int stnc_network_send(
    stnc_network_connection *connection,
    const unsigned char *buffer,
    size_t length
);

int stnc_network_receive(
    stnc_network_connection *connection,
    unsigned char *buffer,
    size_t length
);

void stnc_network_disconnect(
    stnc_network_connection *connection
);

int stnc_network_is_connected(
    const stnc_network_connection *connection
);

#endif
