#ifndef STNC_PLATFORM_H
#define STNC_PLATFORM_H

#include <stddef.h>

int stnc_platform_init(void);
void stnc_platform_shutdown(void);

int stnc_platform_get_app_directory(
    char *buffer,
    size_t buffer_size
);

int stnc_platform_install_stop_handler(void);
void stnc_platform_wait(unsigned int milliseconds);

int stnc_platform_network_init(void);
void stnc_platform_network_shutdown(void);

int stnc_platform_network_connect(
    void **handle,
    const char *peer,
    unsigned short port
);

int stnc_platform_network_send(
    void *handle,
    const unsigned char *buffer,
    size_t length
);

int stnc_platform_network_receive(
    void *handle,
    unsigned char *buffer,
    size_t length
);

void stnc_platform_network_disconnect(
    void *handle
);

#endif
