#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stnc_core.h"
#include "stnc_platform.h"

static int platform_initialized = 0;
static int stop_handler_installed = 0;
static int network_initialized = 0;

#define STNC_SOCKET_IO_TIMEOUT_MS 5000u

static BOOL WINAPI stnc_windows_console_handler(DWORD control_type)
{
    switch (control_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            stnc_core_request_stop();
            return TRUE;

        default:
            return FALSE;
    }
}

int stnc_platform_init(void)
{
    if (platform_initialized) {
        return 1;
    }

    platform_initialized = 1;
    return 0;
}

void stnc_platform_shutdown(void)
{
    if (!platform_initialized) {
        return;
    }

    if (network_initialized) {
        stnc_platform_network_shutdown();
    }

    if (stop_handler_installed) {
        SetConsoleCtrlHandler(stnc_windows_console_handler, FALSE);
        stop_handler_installed = 0;
    }

    platform_initialized = 0;
}

int stnc_platform_get_app_directory(char *buffer, size_t buffer_size)
{
    DWORD length;
    char *separator;

    if (!platform_initialized ||
        buffer == NULL ||
        buffer_size == 0 ||
        buffer_size > MAXDWORD) {
        return 1;
    }

    length = GetModuleFileNameA(NULL, buffer, (DWORD)buffer_size);

    if (length == 0 || (size_t)length >= buffer_size) {
        return 1;
    }

    separator = strrchr(buffer, '\\');
    if (separator == NULL) {
        separator = strrchr(buffer, '/');
    }
    if (separator == NULL) {
        return 1;
    }

    *separator = '\0';
    return 0;
}

int stnc_platform_install_stop_handler(void)
{
    if (!platform_initialized || stop_handler_installed) {
        return 1;
    }

    if (!SetConsoleCtrlHandler(stnc_windows_console_handler, TRUE)) {
        return 1;
    }

    stop_handler_installed = 1;
    return 0;
}

void stnc_platform_wait(unsigned int milliseconds)
{
    Sleep((DWORD)milliseconds);
}

int stnc_platform_network_init(void)
{
    WSADATA data;

    if (!platform_initialized || network_initialized) {
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return 1;
    }

    if (LOBYTE(data.wVersion) != 2 ||
        HIBYTE(data.wVersion) != 2) {
        WSACleanup();
        return 1;
    }

    network_initialized = 1;
    return 0;
}

void stnc_platform_network_shutdown(void)
{
    if (!network_initialized) {
        return;
    }

    WSACleanup();
    network_initialized = 0;
}

int stnc_platform_network_connect(
    void **handle,
    const char *peer,
    unsigned short port
)
{
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *current;
    SOCKET socket_handle;
    char service[6];
    int written;
    int result;

    if (!platform_initialized ||
        !network_initialized ||
        handle == NULL ||
        peer == NULL ||
        peer[0] == '\0' ||
        port == 0) {
        return 1;
    }

    *handle = NULL;

    written = snprintf(service, sizeof(service), "%u", (unsigned int)port);
    if (written < 1 || (size_t)written >= sizeof(service)) {
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    result = getaddrinfo(peer, service, &hints, &results);
    if (result != 0) {
        return 1;
    }

    socket_handle = INVALID_SOCKET;

    for (current = results; current != NULL; current = current->ai_next) {
        socket_handle = socket(
            current->ai_family,
            current->ai_socktype,
            current->ai_protocol
        );

        if (socket_handle == INVALID_SOCKET) {
            continue;
        }

        if (connect(
                socket_handle,
                current->ai_addr,
                (int)current->ai_addrlen
            ) == 0) {
            break;
        }

        closesocket(socket_handle);
        socket_handle = INVALID_SOCKET;
    }

    freeaddrinfo(results);

    if (socket_handle == INVALID_SOCKET) {
        return 1;
    }

    {
        DWORD timeout;

        timeout = (DWORD)STNC_SOCKET_IO_TIMEOUT_MS;

        if (setsockopt(
                socket_handle,
                SOL_SOCKET,
                SO_SNDTIMEO,
                (const char *)&timeout,
                (int)sizeof(timeout)
            ) == SOCKET_ERROR ||
            setsockopt(
                socket_handle,
                SOL_SOCKET,
                SO_RCVTIMEO,
                (const char *)&timeout,
                (int)sizeof(timeout)
            ) == SOCKET_ERROR) {
            closesocket(socket_handle);
            return 1;
        }
    }

    *handle = (void *)(uintptr_t)socket_handle;
    return 0;
}

int stnc_platform_network_send(
    void *handle,
    const unsigned char *buffer,
    size_t length
)
{
    SOCKET socket_handle;
    size_t sent;

    if (!network_initialized ||
        handle == NULL ||
        buffer == NULL ||
        length == 0) {
        return 1;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    sent = 0;

    while (sent < length) {
        int result;
        size_t remaining;

        remaining = length - sent;

        if (remaining > (size_t)INT_MAX) {
            return 1;
        }

        result = send(
            socket_handle,
            (const char *)(buffer + sent),
            (int)remaining,
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return 1;
        }

        sent += (size_t)result;
    }

    return 0;
}

int stnc_platform_network_receive(
    void *handle,
    unsigned char *buffer,
    size_t length
)
{
    SOCKET socket_handle;
    size_t received;

    if (!network_initialized ||
        handle == NULL ||
        buffer == NULL ||
        length == 0) {
        return 1;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    received = 0;

    while (received < length) {
        int result;
        size_t remaining;

        remaining = length - received;

        if (remaining > (size_t)INT_MAX) {
            return 1;
        }

        result = recv(
            socket_handle,
            (char *)(buffer + received),
            (int)remaining,
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return 1;
        }

        received += (size_t)result;
    }

    return 0;
}

void stnc_platform_network_disconnect(void *handle)
{
    SOCKET socket_handle;

    if (!network_initialized || handle == NULL) {
        return;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    shutdown(socket_handle, SD_BOTH);
    closesocket(socket_handle);
}
