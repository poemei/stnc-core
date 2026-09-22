#include <windows.h>

#include <stddef.h>
#include <string.h>

#include "stnc_core.h"
#include "stnc_platform.h"

static int platform_initialized = 0;
static int stop_handler_installed = 0;

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

    if (stop_handler_installed) {
        SetConsoleCtrlHandler(
            stnc_windows_console_handler,
            FALSE
        );

        stop_handler_installed = 0;
    }

    platform_initialized = 0;
}

int stnc_platform_get_app_directory(
    char *buffer,
    size_t buffer_size
)
{
    DWORD length;
    char *separator;

    if (!platform_initialized ||
        buffer == NULL ||
        buffer_size == 0 ||
        buffer_size > MAXDWORD) {
        return 1;
    }

    length = GetModuleFileNameA(
        NULL,
        buffer,
        (DWORD)buffer_size
    );

    if (length == 0 || length >= buffer_size) {
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
    if (!platform_initialized ||
        stop_handler_installed) {
        return 1;
    }

    if (!SetConsoleCtrlHandler(
            stnc_windows_console_handler,
            TRUE
        )) {
        return 1;
    }

    stop_handler_installed = 1;

    return 0;
}

void stnc_platform_wait(unsigned int milliseconds)
{
    Sleep((DWORD)milliseconds);
}