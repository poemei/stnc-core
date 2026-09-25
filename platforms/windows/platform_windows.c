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

int stnc_platform_network_read_ready(void *handle)
{
    SOCKET socket_handle;
    fd_set read_set;
    struct timeval timeout;
    int result;

    if (!network_initialized || handle == NULL) {
        return -1;
    }

    socket_handle = (SOCKET)(uintptr_t)handle;
    FD_ZERO(&read_set);
    FD_SET(socket_handle, &read_set);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    result = select(0, &read_set, NULL, NULL, &timeout);
    if (result == SOCKET_ERROR) {
        return -1;
    }

    return result > 0 && FD_ISSET(socket_handle, &read_set) ? 1 : 0;
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

int stnc_platform_https_get(
    const char *host,
    const char *path,
    char *buffer,
    size_t capacity,
    size_t *length
)
{
    wchar_t wide_host[256];
    wchar_t wide_path[1024];
    HINTERNET session;
    HINTERNET connection;
    HINTERNET request;
    DWORD status;
    DWORD status_size;
    size_t used;
    int result;

    if (length != NULL) {
        *length = 0;
    }

    if (!platform_initialized ||
        host == NULL || host[0] == '\0' ||
        path == NULL || path[0] != '/' ||
        buffer == NULL || capacity < 2u ||
        length == NULL ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, host, -1,
            wide_host, (int)(sizeof(wide_host) / sizeof(wide_host[0]))) == 0 ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
            wide_path, (int)(sizeof(wide_path) / sizeof(wide_path[0]))) == 0) {
        return 1;
    }

    session = WinHttpOpen(
        L"STNC-Core/0.1",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (session == NULL) {
        return 1;
    }

    connection = WinHttpConnect(
        session,
        wide_host,
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (connection == NULL) {
        WinHttpCloseHandle(session);
        return 1;
    }

    request = WinHttpOpenRequest(
        connection,
        L"GET",
        wide_path,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (request == NULL) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return 1;
    }

    result = 1;
    used = 0;

    if (WinHttpSetTimeouts(request, 5000, 5000, 5000, 5000) &&
        WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        ) &&
        WinHttpReceiveResponse(request, NULL)) {
        status = 0;
        status_size = (DWORD)sizeof(status);

        if (WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &status_size,
                WINHTTP_NO_HEADER_INDEX
            ) &&
            status == 200u) {
            for (;;) {
                DWORD received;
                size_t remaining;

                if (used + 1u >= capacity) {
                    break;
                }

                remaining = capacity - used - 1u;

                if (remaining > (size_t)MAXDWORD) {
                    remaining = (size_t)MAXDWORD;
                }

                received = 0;

                if (!WinHttpReadData(
                        request,
                        buffer + used,
                        (DWORD)remaining,
                        &received
                    )) {
                    break;
                }

                if (received == 0u) {
                    buffer[used] = '\0';
                    *length = used;
                    result = 0;
                    break;
                }

                used += (size_t)received;
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return result;
}

int stnc_platform_random(unsigned char *buffer,size_t length)
{
    if(buffer==NULL||length==0||length>(size_t)ULONG_MAX)return 1;
    return BCryptGenRandom(NULL,buffer,(ULONG)length,BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0?0:1;
}

int stnc_platform_sha256(const unsigned char *buffer,size_t length,unsigned char digest[32])
{
    static BCRYPT_ALG_HANDLE algorithm=NULL;
    static BCRYPT_HASH_HANDLE hash=NULL;
    static SRWLOCK lock=SRWLOCK_INIT;
    NTSTATUS status;
    if(buffer==NULL||digest==NULL||length>(size_t)ULONG_MAX)return 1;
    AcquireSRWLockExclusive(&lock);
    if(algorithm==NULL){
        status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,NULL,BCRYPT_HASH_REUSABLE_FLAG);
        if(status!=0){algorithm=NULL;ReleaseSRWLockExclusive(&lock);return 1;}
    }
    if(hash==NULL){
        status=BCryptCreateHash(algorithm,&hash,NULL,0,NULL,0,BCRYPT_HASH_REUSABLE_FLAG);
        if(status!=0){hash=NULL;ReleaseSRWLockExclusive(&lock);return 1;}
    }
    status=BCryptHashData(hash,(PUCHAR)buffer,(ULONG)length,0);
    if(status==0)status=BCryptFinishHash(hash,digest,32,0);
    if(status!=0){
        BCryptDestroyHash(hash);
        hash=NULL;
    }
    ReleaseSRWLockExclusive(&lock);
    return status==0?0:1;
}

void stnc_platform_secure_clear(void *buffer,size_t length){if(buffer!=NULL&&length!=0)SecureZeroMemory(buffer,length);}

char stnc_platform_path_separator(void){return '\\';}


int stnc_platform_write_private_file(const char *path,const unsigned char *buffer,size_t length)
{
    HANDLE token=NULL,file=INVALID_HANDLE_VALUE;DWORD size=0,written=0;TOKEN_USER *user=NULL;
    EXPLICIT_ACCESSA access;PACL acl=NULL;SECURITY_DESCRIPTOR descriptor;SECURITY_ATTRIBUTES attributes;
    DWORD result=ERROR_SUCCESS;int rc=1;
    if(path==NULL||path[0]=='\0'||buffer==NULL||length==0u||length>(size_t)MAXDWORD)return 1;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))goto done;
    GetTokenInformation(token,TokenUser,NULL,0,&size);
    if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER)goto done;
    user=(TOKEN_USER *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);if(user==NULL)goto done;
    if(!GetTokenInformation(token,TokenUser,user,size,&size))goto done;
    ZeroMemory(&access,sizeof(access));access.grfAccessPermissions=GENERIC_ALL;access.grfAccessMode=SET_ACCESS;
    access.grfInheritance=NO_INHERITANCE;access.Trustee.TrusteeForm=TRUSTEE_IS_SID;
    access.Trustee.TrusteeType=TRUSTEE_IS_USER;access.Trustee.ptstrName=(LPSTR)user->User.Sid;
    result=SetEntriesInAclA(1,&access,NULL,&acl);if(result!=ERROR_SUCCESS)goto done;
    if(!InitializeSecurityDescriptor(&descriptor,SECURITY_DESCRIPTOR_REVISION))goto done;
    if(!SetSecurityDescriptorDacl(&descriptor,TRUE,acl,FALSE))goto done;
    attributes.nLength=sizeof(attributes);attributes.lpSecurityDescriptor=&descriptor;attributes.bInheritHandle=FALSE;
    file=CreateFileA(path,GENERIC_WRITE,0,&attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE)goto done;
    if(!WriteFile(file,buffer,(DWORD)length,&written,NULL)||written!=(DWORD)length)goto done;
    if(!FlushFileBuffers(file))goto done;
    rc=0;
done:
    if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);if(rc!=0)DeleteFileA(path);}
    if(acl!=NULL)LocalFree(acl);if(user!=NULL)HeapFree(GetProcessHeap(),0,user);if(token!=NULL)CloseHandle(token);
    return rc;
}

int stnc_platform_protect_private_file(const char *path)
{
    HANDLE token=NULL;DWORD size=0;TOKEN_USER *user=NULL;EXPLICIT_ACCESSA access;PACL acl=NULL;DWORD result=ERROR_SUCCESS;
    if(path==NULL||path[0]=='\0')return 1;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 1;
    GetTokenInformation(token,TokenUser,NULL,0,&size);
    if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER){CloseHandle(token);return 1;}
    user=(TOKEN_USER *)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
    if(user==NULL){CloseHandle(token);return 1;}
    if(!GetTokenInformation(token,TokenUser,user,size,&size)){HeapFree(GetProcessHeap(),0,user);CloseHandle(token);return 1;}
    ZeroMemory(&access,sizeof(access));access.grfAccessPermissions=GENERIC_ALL;access.grfAccessMode=SET_ACCESS;access.grfInheritance=NO_INHERITANCE;
    access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.TrusteeType=TRUSTEE_IS_USER;access.Trustee.ptstrName=(LPSTR)user->User.Sid;
    result=SetEntriesInAclA(1,&access,NULL,&acl);
    if(result==ERROR_SUCCESS)result=SetNamedSecurityInfoA((LPSTR)path,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,NULL,NULL,acl,NULL);
    if(acl!=NULL)LocalFree(acl);HeapFree(GetProcessHeap(),0,user);CloseHandle(token);return result==ERROR_SUCCESS?0:1;
}

uint64_t stnc_platform_monotonic_ms(void)
{
    return (uint64_t)GetTickCount64();
}
