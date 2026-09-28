#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <aclapi.h>

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

/* Chain validation and accepted-state reconstruction can legitimately take
 * longer than five seconds as accepted history grows. A transport timeout is
 * not evidence that Chain is offline. Keep a bounded timeout, but allow a
 * complete deterministic RPC operation enough time to return its response. */
#define STNC_SOCKET_IO_TIMEOUT_MS 30000u

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
    platform_initialized = 0;
}

int stnc_platform_get_app_directory(
    char *buffer,
    size_t buffer_size
)
{
    DWORD length;
    char *last_slash;
    char *last_forward_slash;

    if (!platform_initialized ||
        buffer == NULL ||
        buffer_size == 0) {
        return 1;
    }

    length = GetModuleFileNameA(
        NULL,
        buffer,
        (DWORD)buffer_size
    );

    if (length == 0 ||
        length >= buffer_size) {
        return 1;
    }

    last_slash = strrchr(buffer, '\\');
    last_forward_slash = strrchr(buffer, '/');

    if (last_forward_slash != NULL &&
        (last_slash == NULL || last_forward_slash > last_slash)) {
        last_slash = last_forward_slash;
    }

    if (last_slash == NULL) {
        return 1;
    }

    *last_slash = '\0';

    return 0;
}

static BOOL WINAPI stnc_windows_console_handler(DWORD control_type);

int stnc_platform_install_stop_handler(void)
{
    if (!platform_initialized || stop_handler_installed) {
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

int stnc_platform_network_init(void)
{
    WSADATA data;

    if (!platform_initialized || network_initialized) {
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
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
    char service[16];
    int result;

    if (!network_initialized ||
        handle == NULL ||
        peer == NULL ||
        peer[0] == '\0' ||
        port == 0) {
        return 1;
    }

    *handle = NULL;

    if (snprintf(
            service,
            sizeof(service),
            "%u",
            (unsigned int)port
        ) < 0) {
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

int stnc_platform_network_read_ready(void *handle)
{
    SOCKET socket_handle;
    fd_set read_set;
    struct timeval timeout;
    int result;

    if (!network_initialized || handle == NULL) {
        return 0;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    FD_ZERO(&read_set);
    FD_SET(socket_handle, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    result = select(0, &read_set, NULL, NULL, &timeout);
    return result > 0 && FD_ISSET(socket_handle, &read_set);
}

void stnc_platform_network_disconnect(
    void *handle
)
{
    SOCKET socket_handle;

    if (handle == NULL) {
        return;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    shutdown(socket_handle, SD_BOTH);
    closesocket(socket_handle);
}

int stnc_platform_https_get(
    const char *host,
    const char *path,
    char *buffer,
    size_t capacity,
    size_t *length
)
{
    HINTERNET session;
    HINTERNET connection;
    HINTERNET request;
    DWORD status_code;
    DWORD status_size;
    DWORD available;
    DWORD received;
    size_t used;
    int result;

    if (!platform_initialized ||
        host == NULL ||
        path == NULL ||
        buffer == NULL ||
        capacity == 0 ||
        length == NULL) {
        return 1;
    }

    *length = 0;
    session = NULL;
    connection = NULL;
    request = NULL;
    used = 0;
    result = 1;

    session = WinHttpOpen(
        L"STNC-Core/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (session == NULL) {
        goto cleanup;
    }

    connection = WinHttpConnect(
        session,
        (LPCWSTR)NULL,
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    (void)host;
    (void)path;

cleanup:
    if (request != NULL) WinHttpCloseHandle(request);
    if (connection != NULL) WinHttpCloseHandle(connection);
    if (session != NULL) WinHttpCloseHandle(session);
    return result;
}

int stnc_platform_random(unsigned char *buffer,size_t length)
{
    if(buffer==NULL||length==0u)return 1;
    return BCryptGenRandom(NULL,buffer,(ULONG)length,BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0?0:1;
}

int stnc_platform_sha256(const unsigned char *buffer,size_t length,unsigned char digest[32])
{
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;DWORD object_length=0,result_length=0;PUCHAR object=NULL;NTSTATUS status;
    if(buffer==NULL||digest==NULL||length>ULONG_MAX)return 1;
    status=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0);if(status!=0)goto fail;
    status=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_length,sizeof(object_length),&result_length,0);if(status!=0||object_length==0)goto fail;
    object=(PUCHAR)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,object_length);if(object==NULL)goto fail;
    status=BCryptCreateHash(alg,&hash,object,object_length,NULL,0,0);if(status!=0)goto fail;
    status=BCryptHashData(hash,(PUCHAR)buffer,(ULONG)length,0);if(status!=0)goto fail;
    status=BCryptFinishHash(hash,digest,32,0);if(status!=0)goto fail;
    BCryptDestroyHash(hash);HeapFree(GetProcessHeap(),0,object);BCryptCloseAlgorithmProvider(alg,0);return 0;
fail:
    if(hash!=NULL)BCryptDestroyHash(hash);if(object!=NULL)HeapFree(GetProcessHeap(),0,object);if(alg!=NULL)BCryptCloseAlgorithmProvider(alg,0);return 1;
}

void stnc_platform_secure_clear(void *buffer,size_t length)
{
    if(buffer!=NULL&&length!=0u)SecureZeroMemory(buffer,length);
}

int stnc_platform_protect_private_file(const char *path)
{
    PSID user_sid=NULL;PACL acl=NULL;PSECURITY_DESCRIPTOR descriptor=NULL;DWORD result;
    if(path==NULL||path[0]=='\0')return 1;
    result=GetNamedSecurityInfoA((LPSTR)path,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&user_sid,NULL,NULL,NULL,&descriptor);
    if(result!=ERROR_SUCCESS||user_sid==NULL)goto fail;
    {
        EXPLICIT_ACCESSA access;memset(&access,0,sizeof(access));access.grfAccessPermissions=GENERIC_READ|GENERIC_WRITE|DELETE;access.grfAccessMode=SET_ACCESS;access.grfInheritance=NO_INHERITANCE;access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.TrusteeType=TRUSTEE_IS_USER;access.Trustee.ptstrName=(LPSTR)user_sid;
        result=SetEntriesInAclA(1,&access,NULL,&acl);if(result!=ERROR_SUCCESS)goto fail;
        result=SetNamedSecurityInfoA((LPSTR)path,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,acl,NULL);if(result!=ERROR_SUCCESS)goto fail;
    }
    if(acl!=NULL)LocalFree(acl);if(descriptor!=NULL)LocalFree(descriptor);return 0;
fail:
    if(acl!=NULL)LocalFree(acl);if(descriptor!=NULL)LocalFree(descriptor);return 1;
}

int stnc_platform_write_private_file(const char *path,const unsigned char *buffer,size_t length)
{
    HANDLE file;DWORD written;size_t offset=0u;
    if(path==NULL||buffer==NULL||length==0u)return 1;
    file=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return 1;
    while(offset<length){DWORD chunk=(DWORD)((length-offset)>DWORD_MAX?DWORD_MAX:(length-offset));if(!WriteFile(file,buffer+offset,chunk,&written,NULL)||written!=chunk){CloseHandle(file);DeleteFileA(path);return 1;}offset+=written;}
    if(!FlushFileBuffers(file)){CloseHandle(file);DeleteFileA(path);return 1;}CloseHandle(file);if(stnc_platform_protect_private_file(path)!=0){DeleteFileA(path);return 1;}return 0;
}

char stnc_platform_path_separator(void){return '\\';}
uint64_t stnc_platform_monotonic_ms(void){return (uint64_t)GetTickCount64();}
