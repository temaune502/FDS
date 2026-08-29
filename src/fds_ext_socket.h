/*
    fds_ext_socket.h — Advanced TCP Socket & Stream Wrapper (FDS Extension).
    
    Features:
    - Cross-platform WinSock2 / POSIX abstractions.
    - True Non-Blocking state-machine for framed I/O (no inner loops).
    - Prevents OOM DoS with configurable max packet sizes.
    - getaddrinfo support (hostnames and IPs).
    - Raw I/O struct returns, EINTR handling, async connect completion.

    Usage:
    In EXACTLY ONE C file, add:
       #define FDS_EXT_SOCKET_IMPL
       #include "fds_ext_socket.h"
*/

#ifndef FDS_EXT_SOCKET_H
#define FDS_EXT_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>



// Обмеження пам'яті за замовчуванням: 16 Мегабайтів на пакет
#ifndef FDS_SOCKET_MAX_PACKET_SIZE
#define FDS_SOCKET_MAX_PACKET_SIZE (16 * 1024 * 1024) 
#endif

// Кросплатформове визначення дескриптора
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET fds_raw_socket_t;
    #define FDS_RAW_INVALID_SOCKET INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <netdb.h>
    typedef int fds_raw_socket_t;
    #define FDS_RAW_INVALID_SOCKET (-1)
#endif

// Уніфіковані результати операцій
typedef enum {
    FDS_SOCK_RES_OK = 0,
    FDS_SOCK_RES_CLOSED = 1,
    FDS_SOCK_RES_WOULD_BLOCK = 2,
    FDS_SOCK_RES_ERROR = 3
} FdsSocketResult;

// Результат I/O операцій
typedef struct {
    FdsSocketResult result;
    size_t size; // Кількість байт, які реально були прочитані/записані
} FdsSocketIO;

// Базова обгортка сокета
typedef struct {
    fds_raw_socket_t raw;
    bool is_valid;
} FdsSocket;

// Машина станів для неблокуючого Framed I/O (заміняє fds_socket_send/recv)
typedef struct {
    FdsSocket socket;
    uint32_t max_packet_size;

    // Стан прийому (RX)
    uint32_t rx_packet_size;
    uint32_t rx_received;
    uint8_t  rx_header[4];
    uint32_t rx_header_received;
    bool     rx_active;

    // Стан відправки (TX)
    uint32_t tx_packet_size;
    uint32_t tx_sent;
    uint8_t  tx_header[4];
    uint32_t tx_header_sent;
    bool     tx_active;
    const uint8_t *tx_data; // Вказівник на дані (повинен залишатись валідним під час WOULD_BLOCK!)
} FdsSocketStream;

#ifndef FDS_EXT_SOCKET_DEF
    #define FDS_EXT_SOCKET_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Глобальна ініціалізація
// ============================================================================
FDS_EXT_SOCKET_DEF bool fds_net_init(void);
FDS_EXT_SOCKET_DEF void fds_net_cleanup(void);

// ============================================================================
// Життєвий цикл та з'єднання
// ============================================================================
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create(void);
FDS_EXT_SOCKET_DEF void fds_socket_close(FdsSocket *sock);
FDS_EXT_SOCKET_DEF bool fds_socket_valid(FdsSocket sock);

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_listen(const char *bind_host, uint16_t port, int backlog);
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_connect(const char *host, uint16_t port);
FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_connect_to(FdsSocket sock, const char *host, uint16_t port);
FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_finish_connect(FdsSocket sock); // Для перевірки успіху non-blocking connect

// Покращений accept
FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_accept(FdsSocket listen_sock, FdsSocket *out_client, char *out_client_ip, size_t ip_max_len);

// ============================================================================
// Конфігурація
// ============================================================================
FDS_EXT_SOCKET_DEF bool fds_socket_set_blocking(FdsSocket sock, bool is_blocking);
FDS_EXT_SOCKET_DEF bool fds_socket_set_timeout(FdsSocket sock, uint32_t recv_ms, uint32_t send_ms); // 0 = disable

// ============================================================================
// Мультиплексування
// ============================================================================
FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_poll(FdsSocket sock, int timeout_ms, bool *can_read, bool *can_write);

// ============================================================================
// Raw I/O (Для кастомних event loops без фреймів)
// ============================================================================
FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_send_raw(FdsSocket sock, const void *data, size_t size);
FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_recv_raw(FdsSocket sock, void *buffer, size_t size);

// ============================================================================
// Framed Stream I/O (Машина станів)
// ============================================================================
FDS_EXT_SOCKET_DEF void fds_stream_init(FdsSocketStream *stream, FdsSocket sock, uint32_t max_packet_size);
FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_send(FdsSocketStream *stream, FdsBytesView packet);
FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_recv(FdsSocketStream *stream, FdsBytesBuilder *out_builder);
FDS_EXT_SOCKET_DEF void fds_stream_reset_rx(FdsSocketStream *stream); // Скинути стан після помилки
FDS_EXT_SOCKET_DEF void fds_stream_reset_tx(FdsSocketStream *stream);

#ifdef __cplusplus
}
#endif

#endif // FDS_EXT_SOCKET_H

/* ============================================================================
   IMPL
   ============================================================================ */
#ifdef FDS_EXT_SOCKET_IMPL
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
    #define FDS_SOCK_BUF(ptr) ((const char *)(ptr))
    #define FDS_SOCK_MUT_BUF(ptr) ((char *)(ptr))
    #define FDS_CLOSE_CALL(s) closesocket(s)
#else
    #define FDS_SOCK_BUF(ptr) ((const void *)(ptr))
    #define FDS_SOCK_MUT_BUF(ptr) ((void *)(ptr))
    #define FDS_CLOSE_CALL(s) close(s)
#endif

static bool fds_internal_is_block_error(void) {
#ifdef _WIN32
    int err = WSAGetLastError();
    return (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS);
#else
    return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS);
#endif
}

FDS_EXT_SOCKET_DEF bool fds_net_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

FDS_EXT_SOCKET_DEF void fds_net_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create(void) {
    FdsSocket result = { .raw = FDS_RAW_INVALID_SOCKET, .is_valid = false };
    fds_raw_socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock != FDS_RAW_INVALID_SOCKET) {
        result.raw = sock;
        result.is_valid = true;
    }
    return result;
}

FDS_EXT_SOCKET_DEF void fds_socket_close(FdsSocket *sock) {
    if (!sock || !sock->is_valid) return;
    FDS_CLOSE_CALL(sock->raw);
    sock->raw = FDS_RAW_INVALID_SOCKET;
    sock->is_valid = false;
}

FDS_EXT_SOCKET_DEF bool fds_socket_valid(FdsSocket sock) {
    return sock.is_valid && sock.raw != FDS_RAW_INVALID_SOCKET;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_listen(const char *bind_host, uint16_t port, int backlog) {
    FdsSocket result = { .raw = FDS_RAW_INVALID_SOCKET, .is_valid = false };
    
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    // Якщо bind_host == NULL, буде прив'язка до INADDR_ANY (0.0.0.0)
    if (getaddrinfo(bind_host, port_str, &hints, &res) != 0) return result;

    fds_raw_socket_t sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == FDS_RAW_INVALID_SOCKET) {
        freeaddrinfo(res);
        return result;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, FDS_SOCK_BUF(&opt), sizeof(opt));

    if (bind(sock, res->ai_addr, (int)res->ai_addrlen) == -1 || listen(sock, backlog <= 0 ? SOMAXCONN : backlog) == -1) {
        FDS_CLOSE_CALL(sock);
        freeaddrinfo(res);
        return result;
    }

    freeaddrinfo(res);
    result.raw = sock;
    result.is_valid = true;
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_connect_to(FdsSocket sock, const char *host, uint16_t port) {
    if (!fds_socket_valid(sock) || !host) return FDS_SOCK_RES_ERROR;

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET; // Оскільки create робить AF_INET
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) return FDS_SOCK_RES_ERROR;

    int r = connect(sock.raw, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    if (r == -1) {
        if (fds_internal_is_block_error()) return FDS_SOCK_RES_WOULD_BLOCK;
        return FDS_SOCK_RES_ERROR;
    }
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_connect(const char *host, uint16_t port) {
    FdsSocket result = fds_socket_create();
    if (result.is_valid) {
        FdsSocketResult res = fds_socket_connect_to(result, host, port);
        if (res == FDS_SOCK_RES_ERROR) {
            fds_socket_close(&result);
        }
    }
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_finish_connect(FdsSocket sock) {
    if (!fds_socket_valid(sock)) return FDS_SOCK_RES_ERROR;
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(sock.raw, SOL_SOCKET, SO_ERROR, FDS_SOCK_MUT_BUF(&err), &len) < 0) {
        return FDS_SOCK_RES_ERROR;
    }
    if (err != 0) return FDS_SOCK_RES_ERROR;
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_accept(FdsSocket listen_sock, FdsSocket *out_client, char *out_client_ip, size_t ip_max_len) {
    if (!out_client) return FDS_SOCK_RES_ERROR;
    out_client->is_valid = false;
    out_client->raw = FDS_RAW_INVALID_SOCKET;
    
    if (!fds_socket_valid(listen_sock)) return FDS_SOCK_RES_ERROR;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    fds_raw_socket_t client_sock = accept(listen_sock.raw, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_sock == FDS_RAW_INVALID_SOCKET) {
        if (fds_internal_is_block_error()) return FDS_SOCK_RES_WOULD_BLOCK;
        return FDS_SOCK_RES_ERROR;
    }

    if (out_client_ip && ip_max_len > 0) {
        const char *ip_str = inet_ntop(AF_INET, &client_addr.sin_addr, out_client_ip, (socklen_t)ip_max_len);
        if (!ip_str) out_client_ip[0] = '\0';
    }

    out_client->raw = client_sock;
    out_client->is_valid = true;
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF bool fds_socket_set_blocking(FdsSocket sock, bool is_blocking) {
    if (!fds_socket_valid(sock)) return false;
#ifdef _WIN32
    u_long mode = is_blocking ? 0 : 1;
    return ioctlsocket(sock.raw, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock.raw, F_GETFL, 0);
    if (flags < 0) return false;
    flags = is_blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(sock.raw, F_SETFL, flags) == 0;
#endif
}

FDS_EXT_SOCKET_DEF bool fds_socket_set_timeout(FdsSocket sock, uint32_t recv_ms, uint32_t send_ms) {
    if (!fds_socket_valid(sock)) return false;
    bool ok = true;
#ifdef _WIN32
    DWORD r_to = recv_ms, s_to = send_ms;
    ok &= (setsockopt(sock.raw, SOL_SOCKET, SO_RCVTIMEO, (const char*)&r_to, sizeof(r_to)) == 0);
    ok &= (setsockopt(sock.raw, SOL_SOCKET, SO_SNDTIMEO, (const char*)&s_to, sizeof(s_to)) == 0);
#else
    struct timeval r_tv = { (time_t)(recv_ms / 1000), (suseconds_t)((recv_ms % 1000) * 1000) };
    struct timeval s_tv = { (time_t)(send_ms / 1000), (suseconds_t)((send_ms % 1000) * 1000) };
    ok &= (setsockopt(sock.raw, SOL_SOCKET, SO_RCVTIMEO, &r_tv, sizeof(r_tv)) == 0);
    ok &= (setsockopt(sock.raw, SOL_SOCKET, SO_SNDTIMEO, &s_tv, sizeof(s_tv)) == 0);
#endif
    return ok;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_poll(FdsSocket sock, int timeout_ms, bool *can_read, bool *can_write) {
    if (!fds_socket_valid(sock)) return FDS_SOCK_RES_ERROR;
    if (!can_read && !can_write) return FDS_SOCK_RES_ERROR;
    
    fd_set read_fds, write_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    
    if (can_read) FD_SET(sock.raw, &read_fds);
    if (can_write) FD_SET(sock.raw, &write_fds);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int res;
    do {
        res = select((int)sock.raw + 1, 
                     can_read ? &read_fds : NULL, 
                     can_write ? &write_fds : NULL, 
                     NULL, 
                     timeout_ms >= 0 ? &tv : NULL);
    } while (res < 0 && errno == EINTR); // POSIX Restart on interrupt
                     
    if (res < 0) return FDS_SOCK_RES_ERROR;
    if (res == 0) return FDS_SOCK_RES_WOULD_BLOCK; // Timeout
    
    if (can_read) *can_read = FD_ISSET(sock.raw, &read_fds);
    if (can_write) *can_write = FD_ISSET(sock.raw, &write_fds);
    
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_send_raw(FdsSocket sock, const void *data, size_t size) {
    FdsSocketIO io = { FDS_SOCK_RES_ERROR, 0 };
    if (!fds_socket_valid(sock) || !data || size == 0) return io;
    
    if (size > INT_MAX) size = INT_MAX;

    int r;
    do {
        r = send(sock.raw, FDS_SOCK_BUF(data), (int)size, 0);
    } while (r < 0 && errno == EINTR);

    if (r > 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;
    } else if (r == 0) {
        io.result = FDS_SOCK_RES_CLOSED;
    } else {
        io.result = fds_internal_is_block_error() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }
    return io;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_recv_raw(FdsSocket sock, void *buffer, size_t size) {
    FdsSocketIO io = { FDS_SOCK_RES_ERROR, 0 };
    if (!fds_socket_valid(sock) || !buffer || size == 0) return io;

    if (size > INT_MAX) size = INT_MAX;

    int r;
    do {
        r = recv(sock.raw, FDS_SOCK_MUT_BUF(buffer), (int)size, 0);
    } while (r < 0 && errno == EINTR);

    if (r > 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;
    } else if (r == 0) {
        io.result = FDS_SOCK_RES_CLOSED; // EOF = Clean remote close
    } else {
        io.result = fds_internal_is_block_error() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }
    return io;
}

// ============================================================================
// State Machine (Framed I/O)
// ============================================================================

FDS_EXT_SOCKET_DEF void fds_stream_init(FdsSocketStream *stream, FdsSocket sock, uint32_t max_packet_size) {
    memset(stream, 0, sizeof(*stream));
    stream->socket = sock;
    stream->max_packet_size = (max_packet_size > 0) ? max_packet_size : FDS_SOCKET_MAX_PACKET_SIZE;
}

FDS_EXT_SOCKET_DEF void fds_stream_reset_rx(FdsSocketStream *stream) {
    stream->rx_active = false;
    stream->rx_header_received = 0;
    stream->rx_received = 0;
}

FDS_EXT_SOCKET_DEF void fds_stream_reset_tx(FdsSocketStream *stream) {
    stream->tx_active = false;
    stream->tx_header_sent = 0;
    stream->tx_sent = 0;
    stream->tx_data = NULL;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_send(FdsSocketStream *stream, FdsBytesView packet) {
    if (!fds_socket_valid(stream->socket)) return FDS_SOCK_RES_ERROR;
    if (packet.size > stream->max_packet_size || packet.size > UINT32_MAX) return FDS_SOCK_RES_ERROR;

    // 1. Починаємо новий фрейм
    if (!stream->tx_active) {
        stream->tx_active = true;
        stream->tx_packet_size = (uint32_t)packet.size;
        stream->tx_data = packet.data;
        stream->tx_sent = 0;
        stream->tx_header_sent = 0;

        // Кодуємо Little-Endian
        uint32_t le_size = stream->tx_packet_size;
        stream->tx_header[0] = (uint8_t)(le_size);
        stream->tx_header[1] = (uint8_t)(le_size >> 8);
        stream->tx_header[2] = (uint8_t)(le_size >> 16);
        stream->tx_header[3] = (uint8_t)(le_size >> 24);
    }

    // 2. Відправка заголовка (якщо ще не відправлено повністю)
    while (stream->tx_header_sent < 4) {
        FdsSocketIO io = fds_socket_send_raw(stream->socket, 
                                             stream->tx_header + stream->tx_header_sent, 
                                             4 - stream->tx_header_sent);
        if (io.result != FDS_SOCK_RES_OK) return io.result; // WOULD_BLOCK, CLOSED or ERROR
        stream->tx_header_sent += (uint32_t)io.size;
    }

    // 3. Відправка тіла
    while (stream->tx_sent < stream->tx_packet_size) {
        FdsSocketIO io = fds_socket_send_raw(stream->socket, 
                                             stream->tx_data + stream->tx_sent, 
                                             stream->tx_packet_size - stream->tx_sent);
        if (io.result != FDS_SOCK_RES_OK) return io.result;
        stream->tx_sent += (uint32_t)io.size;
    }

    // 4. Фрейм повністю відправлено
    fds_stream_reset_tx(stream);
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_recv(FdsSocketStream *stream, FdsBytesBuilder *out_builder) {
    if (!fds_socket_valid(stream->socket) || !out_builder) return FDS_SOCK_RES_ERROR;

    // 1. Читаємо заголовок
    if (!stream->rx_active) {
        while (stream->rx_header_received < 4) {
            FdsSocketIO io = fds_socket_recv_raw(stream->socket, 
                                                 stream->rx_header + stream->rx_header_received, 
                                                 4 - stream->rx_header_received);
            if (io.result != FDS_SOCK_RES_OK) return io.result;
            stream->rx_header_received += (uint32_t)io.size;
        }

        // Декодуємо Little-Endian
        uint32_t le_size = ((uint32_t)stream->rx_header[0]) | 
                           ((uint32_t)stream->rx_header[1] << 8) |
                           ((uint32_t)stream->rx_header[2] << 16) | 
                           ((uint32_t)stream->rx_header[3] << 24);

        if (le_size > stream->max_packet_size) {
            fds_stream_reset_rx(stream);
            return FDS_SOCK_RES_ERROR; // Захист від DoS
        }

        stream->rx_packet_size = le_size;
        stream->rx_active = true;
        stream->rx_received = 0;
        
        // Виділяємо пам'ять через FDS Builder
        fds_bb_reserve(out_builder, stream->rx_packet_size);
    }

    // 2. Читаємо тіло
    while (stream->rx_received < stream->rx_packet_size) {
        FdsSocketIO io = fds_socket_recv_raw(stream->socket, 
                                             out_builder->data + out_builder->size + stream->rx_received, 
                                             stream->rx_packet_size - stream->rx_received);
        if (io.result != FDS_SOCK_RES_OK) {
            // Якщо обрив з'єднання під час читання тіла — це FATAL (Stream Corruption)
            if (io.result == FDS_SOCK_RES_CLOSED) {
                fds_stream_reset_rx(stream);
                return FDS_SOCK_RES_ERROR; 
            }
            return io.result; // WOULD_BLOCK
        }
        stream->rx_received += (uint32_t)io.size;
    }

    // 3. Фрейм повністю прочитано
    out_builder->size += stream->rx_packet_size;
    fds_stream_reset_rx(stream);
    
    return FDS_SOCK_RES_OK;
}

#endif // FDS_EXT_SOCKET_IMPL