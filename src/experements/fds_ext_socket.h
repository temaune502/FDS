/*
    fds_ext_socket.h — Cross-platform low-level TCP / UDP sockets (FDS Extension).

    The library intentionally stays at the transport layer:
    - TCP and UDP
    - IPv4 / IPv6 address resolution
    - blocking / non-blocking mode
    - socket timeouts
    - readiness polling
    - raw TCP I/O
    - UDP sendto / recvfrom
    - framed TCP stream helper

    HTTPS / HTTP is intentionally NOT implemented here. Use a higher-level
    library such as libcurl for HTTP(S) file transfers.

    In EXACTLY ONE C file:
        #define FDS_EXT_SOCKET_IMPL
        #include "fds_ext_socket.h"
*/

#ifndef FDS_EXT_SOCKET_H
#define FDS_EXT_SOCKET_H

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200112L
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#ifndef FDS_SOCKET_MAX_PACKET_SIZE
#define FDS_SOCKET_MAX_PACKET_SIZE (16 * 1024 * 1024)
#endif

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
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
    #include <time.h>
    #include <sys/time.h>
    typedef int fds_raw_socket_t;
    #define FDS_RAW_INVALID_SOCKET (-1)
#endif

typedef enum {
    FDS_SOCK_RES_OK = 0,
    FDS_SOCK_RES_CLOSED = 1,
    FDS_SOCK_RES_WOULD_BLOCK = 2,
    FDS_SOCK_RES_ERROR = 3
} FdsSocketResult;

typedef struct {
    FdsSocketResult result;
    size_t size;
} FdsSocketIO;

typedef enum {
    FDS_SOCKET_TCP = 1,
    FDS_SOCKET_UDP = 2
} FdsSocketType;

/*
    Owns no resources. It is simply a portable sockaddr_storage wrapper.
    `length` is the actual sockaddr length in bytes.
*/
typedef struct {
    struct sockaddr_storage storage;
    int length;
} FdsSocketAddress;

typedef struct {
    fds_raw_socket_t raw;
    bool is_valid;
    FdsSocketType type;
    int family;
} FdsSocket;

typedef struct {
    FdsSocket socket;
    uint32_t max_packet_size;

    uint32_t rx_packet_size;
    uint32_t rx_received;
    uint8_t  rx_header[4];
    uint32_t rx_header_received;
    bool     rx_active;

    uint32_t tx_packet_size;
    uint32_t tx_sent;
    uint8_t  tx_header[4];
    uint32_t tx_header_sent;
    bool     tx_active;
    const uint8_t *tx_data;
} FdsSocketStream;

#ifndef FDS_EXT_SOCKET_DEF
    #define FDS_EXT_SOCKET_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   Global networking
   ============================================================================ */
FDS_EXT_SOCKET_DEF bool fds_net_init(void);
FDS_EXT_SOCKET_DEF void fds_net_cleanup(void);

/* ============================================================================
   Socket lifecycle
   ============================================================================ */
/* Compatibility: creates an IPv4 TCP socket. */
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create(void);
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create_tcp(int family);
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create_udp(int family);

FDS_EXT_SOCKET_DEF void fds_socket_close(FdsSocket *sock);
FDS_EXT_SOCKET_DEF bool fds_socket_valid(FdsSocket sock);

/* ============================================================================
   Address helpers
   ============================================================================ */
FDS_EXT_SOCKET_DEF bool fds_socket_resolve(
    const char *host,
    uint16_t port,
    FdsSocketType type,
    FdsSocketAddress *out_address);

FDS_EXT_SOCKET_DEF bool fds_socket_address_to_string(
    const FdsSocketAddress *address,
    char *out,
    size_t out_size);

FDS_EXT_SOCKET_DEF uint16_t fds_socket_address_port(
    const FdsSocketAddress *address);

/* ============================================================================
   TCP
   ============================================================================ */
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_listen(
    const char *bind_host,
    uint16_t port,
    int backlog);

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_connect(
    const char *host,
    uint16_t port);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_connect_to(
    FdsSocket sock,
    const char *host,
    uint16_t port);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_finish_connect(FdsSocket sock);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_accept(
    FdsSocket listen_sock,
    FdsSocket *out_client,
    char *out_client_ip,
    size_t ip_max_len);

/* ============================================================================
   UDP
   ============================================================================ */
FDS_EXT_SOCKET_DEF FdsSocket fds_socket_udp_bind(
    const char *bind_host,
    uint16_t port);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_udp_connect(
    FdsSocket sock,
    const FdsSocketAddress *remote);

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_send_to(
    FdsSocket sock,
    const FdsSocketAddress *destination,
    const void *data,
    size_t size);

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_recv_from(
    FdsSocket sock,
    void *buffer,
    size_t size,
    FdsSocketAddress *out_source);

/* Convenience wrapper: resolves host and sends one datagram. */
FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_send_host(
    FdsSocket sock,
    const char *host,
    uint16_t port,
    const void *data,
    size_t size);

/* ============================================================================
   Configuration
   ============================================================================ */
FDS_EXT_SOCKET_DEF bool fds_socket_set_blocking(FdsSocket sock, bool is_blocking);
FDS_EXT_SOCKET_DEF bool fds_socket_set_timeout(FdsSocket sock, uint32_t recv_ms, uint32_t send_ms);

/* ============================================================================
   Multiplexing
   ============================================================================ */
FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_poll(
    FdsSocket sock,
    int timeout_ms,
    bool *can_read,
    bool *can_write);

/* ============================================================================
   Raw I/O
   ============================================================================ */
/* Raw send/recv work for TCP and for a UDP socket that was connected with
   fds_socket_udp_connect(). For unconnected UDP use send_to/recv_from. */
FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_send_raw(
    FdsSocket sock,
    const void *data,
    size_t size);

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_recv_raw(
    FdsSocket sock,
    void *buffer,
    size_t size);

/* ============================================================================
   Framed TCP stream helper
   ============================================================================ */
FDS_EXT_SOCKET_DEF void fds_stream_init(
    FdsSocketStream *stream,
    FdsSocket sock,
    uint32_t max_packet_size);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_send(
    FdsSocketStream *stream,
    FdsBytesView packet);

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_recv(
    FdsSocketStream *stream,
    FdsBytesBuilder *out_builder);

FDS_EXT_SOCKET_DEF void fds_stream_reset_rx(FdsSocketStream *stream);
FDS_EXT_SOCKET_DEF void fds_stream_reset_tx(FdsSocketStream *stream);

#ifdef __cplusplus
}
#endif

#endif /* FDS_EXT_SOCKET_H */

/* ============================================================================
   IMPLEMENTATION
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

static FdsSocket fds_socket_invalid(void) {
    FdsSocket result;
    result.raw = FDS_RAW_INVALID_SOCKET;
    result.is_valid = false;
    result.type = 0;
    result.family = AF_UNSPEC;
    return result;
}

static bool fds_internal_is_would_block(void) {
#ifdef _WIN32
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EALREADY;
#endif
}

static bool fds_internal_is_interrupted(void) {
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

static FdsSocketIO fds_internal_io_error(void) {
    FdsSocketIO io;
    io.result = FDS_SOCK_RES_ERROR;
    io.size = 0;
    return io;
}

static FdsSocketResult fds_internal_socket_error(void) {
    return fds_internal_is_would_block() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
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

static FdsSocket fds_internal_socket_create(int family, int socktype, int protocol, FdsSocketType type) {
    FdsSocket result = fds_socket_invalid();
    fds_raw_socket_t raw = socket(family, socktype, protocol);
    if (raw != FDS_RAW_INVALID_SOCKET) {
        result.raw = raw;
        result.is_valid = true;
        result.type = type;
        result.family = family;
    }
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create(void) {
    return fds_socket_create_tcp(AF_INET);
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create_tcp(int family) {
    if (family != AF_INET && family != AF_INET6) return fds_socket_invalid();
    return fds_internal_socket_create(family, SOCK_STREAM, IPPROTO_TCP, FDS_SOCKET_TCP);
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_create_udp(int family) {
    if (family != AF_INET && family != AF_INET6) return fds_socket_invalid();
    return fds_internal_socket_create(family, SOCK_DGRAM, IPPROTO_UDP, FDS_SOCKET_UDP);
}

FDS_EXT_SOCKET_DEF void fds_socket_close(FdsSocket *sock) {
    if (!sock || !sock->is_valid) return;
    FDS_CLOSE_CALL(sock->raw);
    sock->raw = FDS_RAW_INVALID_SOCKET;
    sock->is_valid = false;
    sock->type = 0;
    sock->family = AF_UNSPEC;
}

FDS_EXT_SOCKET_DEF bool fds_socket_valid(FdsSocket sock) {
    return sock.is_valid && sock.raw != FDS_RAW_INVALID_SOCKET;
}

static void fds_internal_fill_hints(struct addrinfo *hints, FdsSocketType type) {
    memset(hints, 0, sizeof(*hints));
    hints->ai_family = AF_UNSPEC;
    hints->ai_socktype = type == FDS_SOCKET_TCP ? SOCK_STREAM : SOCK_DGRAM;
    hints->ai_protocol = type == FDS_SOCKET_TCP ? IPPROTO_TCP : IPPROTO_UDP;
}

FDS_EXT_SOCKET_DEF bool fds_socket_resolve(
    const char *host,
    uint16_t port,
    FdsSocketType type,
    FdsSocketAddress *out_address) {
    if (!out_address || (type != FDS_SOCKET_TCP && type != FDS_SOCKET_UDP)) return false;

    memset(out_address, 0, sizeof(*out_address));

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    fds_internal_fill_hints(&hints, type);

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) return false;

    if (res->ai_addrlen > sizeof(out_address->storage)) {
        freeaddrinfo(res);
        return false;
    }

    memcpy(&out_address->storage, res->ai_addr, (size_t)res->ai_addrlen);
    out_address->length = (int)res->ai_addrlen;
    freeaddrinfo(res);
    return true;
}

FDS_EXT_SOCKET_DEF bool fds_socket_address_to_string(
    const FdsSocketAddress *address,
    char *out,
    size_t out_size) {
    if (!address || !out || out_size == 0 || address->length <= 0) return false;

    const void *src = NULL;
    int family = ((const struct sockaddr *)&address->storage)->sa_family;

    if (family == AF_INET) {
        src = &((const struct sockaddr_in *)&address->storage)->sin_addr;
    } else if (family == AF_INET6) {
        src = &((const struct sockaddr_in6 *)&address->storage)->sin6_addr;
    } else {
        out[0] = '\0';
        return false;
    }

    return inet_ntop(family, src, out,
#ifdef _WIN32
        (DWORD)out_size
#else
        out_size
#endif
    ) != NULL;
}

FDS_EXT_SOCKET_DEF uint16_t fds_socket_address_port(const FdsSocketAddress *address) {
    if (!address || address->length <= 0) return 0;

    const struct sockaddr *sa = (const struct sockaddr *)&address->storage;
    if (sa->sa_family == AF_INET) {
        return ntohs(((const struct sockaddr_in *)sa)->sin_port);
    }
    if (sa->sa_family == AF_INET6) {
        return ntohs(((const struct sockaddr_in6 *)sa)->sin6_port);
    }
    return 0;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_listen(
    const char *bind_host,
    uint16_t port,
    int backlog) {
    FdsSocket result = fds_socket_invalid();

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    fds_internal_fill_hints(&hints, FDS_SOCKET_TCP);
    hints.ai_flags = AI_PASSIVE;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    if (getaddrinfo(bind_host, port_str, &hints, &res) != 0 || !res) return result;

    for (struct addrinfo *it = res; it; it = it->ai_next) {
        fds_raw_socket_t raw = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (raw == FDS_RAW_INVALID_SOCKET) continue;

        int opt = 1;
        (void)setsockopt(raw, SOL_SOCKET, SO_REUSEADDR, FDS_SOCK_BUF(&opt), sizeof(opt));

        if (bind(raw, it->ai_addr, (int)it->ai_addrlen) != 0 ||
            listen(raw, backlog <= 0 ? SOMAXCONN : backlog) != 0) {
            FDS_CLOSE_CALL(raw);
            continue;
        }

        result.raw = raw;
        result.is_valid = true;
        result.type = FDS_SOCKET_TCP;
        result.family = it->ai_family;
        break;
    }

    freeaddrinfo(res);
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_connect(
    const char *host,
    uint16_t port) {
    FdsSocket result = fds_socket_invalid();
    if (!host) return result;

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    fds_internal_fill_hints(&hints, FDS_SOCKET_TCP);

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) return result;

    for (struct addrinfo *it = res; it; it = it->ai_next) {
        fds_raw_socket_t raw = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (raw == FDS_RAW_INVALID_SOCKET) continue;

        int r;
        do {
            r = connect(raw, it->ai_addr, (int)it->ai_addrlen);
        } while (r != 0 && fds_internal_is_interrupted());

        if (r == 0) {
            result.raw = raw;
            result.is_valid = true;
            result.type = FDS_SOCKET_TCP;
            result.family = it->ai_family;
            break;
        }

        FDS_CLOSE_CALL(raw);
    }

    freeaddrinfo(res);
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_connect_to(
    FdsSocket sock,
    const char *host,
    uint16_t port) {
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_TCP || !host) {
        return FDS_SOCK_RES_ERROR;
    }

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    fds_internal_fill_hints(&hints, FDS_SOCKET_TCP);
    hints.ai_family = sock.family == AF_UNSPEC ? AF_UNSPEC : sock.family;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        return FDS_SOCK_RES_ERROR;
    }

    int r;
    do {
        r = connect(sock.raw, res->ai_addr, (int)res->ai_addrlen);
    } while (r != 0 && fds_internal_is_interrupted());

    freeaddrinfo(res);

    if (r == 0) return FDS_SOCK_RES_OK;
    return fds_internal_socket_error();
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_finish_connect(FdsSocket sock) {
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_TCP) return FDS_SOCK_RES_ERROR;

    int err = 0;
#ifdef _WIN32
    int len = (int)sizeof(err);
#else
    socklen_t len = (socklen_t)sizeof(err);
#endif

    if (getsockopt(sock.raw, SOL_SOCKET, SO_ERROR, FDS_SOCK_MUT_BUF(&err), &len) < 0) {
        return FDS_SOCK_RES_ERROR;
    }

    if (err == 0) return FDS_SOCK_RES_OK;
#ifdef _WIN32
    if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY) {
        return FDS_SOCK_RES_WOULD_BLOCK;
    }
#else
    if (err == EINPROGRESS || err == EALREADY || err == EWOULDBLOCK) {
        return FDS_SOCK_RES_WOULD_BLOCK;
    }
#endif
    return FDS_SOCK_RES_ERROR;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_accept(
    FdsSocket listen_sock,
    FdsSocket *out_client,
    char *out_client_ip,
    size_t ip_max_len) {
    if (!out_client) return FDS_SOCK_RES_ERROR;

    *out_client = fds_socket_invalid();
    if (!fds_socket_valid(listen_sock) || listen_sock.type != FDS_SOCKET_TCP) {
        return FDS_SOCK_RES_ERROR;
    }

    struct sockaddr_storage client_addr;
#ifdef _WIN32
    int client_len = (int)sizeof(client_addr);
#else
    socklen_t client_len = (socklen_t)sizeof(client_addr);
#endif

    fds_raw_socket_t client_sock;
    do {
        client_sock = accept(listen_sock.raw,
                             (struct sockaddr *)&client_addr,
                             &client_len);
    } while (client_sock == FDS_RAW_INVALID_SOCKET && fds_internal_is_interrupted());

    if (client_sock == FDS_RAW_INVALID_SOCKET) {
        return fds_internal_socket_error();
    }

    out_client->raw = client_sock;
    out_client->is_valid = true;
    out_client->type = FDS_SOCKET_TCP;
    out_client->family = ((struct sockaddr *)&client_addr)->sa_family;

    if (out_client_ip && ip_max_len > 0) {
        FdsSocketAddress address;
        memset(&address, 0, sizeof(address));
        memcpy(&address.storage, &client_addr, (size_t)client_len);
        address.length = client_len;
        if (!fds_socket_address_to_string(&address, out_client_ip, ip_max_len)) {
            out_client_ip[0] = '\0';
        }
    }

    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocket fds_socket_udp_bind(
    const char *bind_host,
    uint16_t port) {
    FdsSocket result = fds_socket_invalid();

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    fds_internal_fill_hints(&hints, FDS_SOCKET_UDP);
    hints.ai_flags = AI_PASSIVE;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    if (getaddrinfo(bind_host, port_str, &hints, &res) != 0 || !res) return result;

    for (struct addrinfo *it = res; it; it = it->ai_next) {
        fds_raw_socket_t raw = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (raw == FDS_RAW_INVALID_SOCKET) continue;

        int opt = 1;
        (void)setsockopt(raw, SOL_SOCKET, SO_REUSEADDR, FDS_SOCK_BUF(&opt), sizeof(opt));

        if (bind(raw, it->ai_addr, (int)it->ai_addrlen) != 0) {
            FDS_CLOSE_CALL(raw);
            continue;
        }

        result.raw = raw;
        result.is_valid = true;
        result.type = FDS_SOCKET_UDP;
        result.family = it->ai_family;
        break;
    }

    freeaddrinfo(res);
    return result;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_udp_connect(
    FdsSocket sock,
    const FdsSocketAddress *remote) {
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_UDP || !remote || remote->length <= 0) {
        return FDS_SOCK_RES_ERROR;
    }

    int r;
    do {
        r = connect(sock.raw,
                    (const struct sockaddr *)&remote->storage,
                    remote->length);
    } while (r != 0 && fds_internal_is_interrupted());

    if (r == 0) return FDS_SOCK_RES_OK;
    return fds_internal_socket_error();
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_send_to(
    FdsSocket sock,
    const FdsSocketAddress *destination,
    const void *data,
    size_t size) {
    FdsSocketIO io = fds_internal_io_error();
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_UDP ||
        !destination || destination->length <= 0 || (!data && size > 0)) {
        return io;
    }

    if (size > INT_MAX) size = INT_MAX;

    int r;
    do {
        r = sendto(sock.raw,
                   FDS_SOCK_BUF(data),
                   (int)size,
                   0,
                   (const struct sockaddr *)&destination->storage,
                   destination->length);
    } while (r < 0 && fds_internal_is_interrupted());

    if (r >= 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;
    } else {
        io.result = fds_internal_is_would_block() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }

    return io;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_recv_from(
    FdsSocket sock,
    void *buffer,
    size_t size,
    FdsSocketAddress *out_source) {
    FdsSocketIO io = fds_internal_io_error();
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_UDP ||
        !buffer || size == 0) {
        return io;
    }

    if (size > INT_MAX) size = INT_MAX;

    struct sockaddr_storage source;
#ifdef _WIN32
    int source_len = (int)sizeof(source);
#else
    socklen_t source_len = (socklen_t)sizeof(source);
#endif

    int r;
    do {
        r = recvfrom(sock.raw,
                     FDS_SOCK_MUT_BUF(buffer),
                     (int)size,
                     0,
                     (struct sockaddr *)&source,
                     &source_len);
    } while (r < 0 && fds_internal_is_interrupted());

    if (r >= 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;

        if (out_source) {
            if ((size_t)source_len <= sizeof(out_source->storage)) {
                memcpy(&out_source->storage, &source, (size_t)source_len);
                out_source->length = (int)source_len;
            } else {
                out_source->length = 0;
            }
        }
    } else {
        io.result = fds_internal_is_would_block() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }

    return io;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_udp_send_host(
    FdsSocket sock,
    const char *host,
    uint16_t port,
    const void *data,
    size_t size) {
    FdsSocketIO io = fds_internal_io_error();
    if (!fds_socket_valid(sock) || sock.type != FDS_SOCKET_UDP || !host) return io;

    FdsSocketAddress destination;
    if (!fds_socket_resolve(host, port, FDS_SOCKET_UDP, &destination)) return io;
    return fds_socket_udp_send_to(sock, &destination, data, size);
}

FDS_EXT_SOCKET_DEF bool fds_socket_set_blocking(FdsSocket sock, bool is_blocking) {
    if (!fds_socket_valid(sock)) return false;
#ifdef _WIN32
    u_long mode = is_blocking ? 0UL : 1UL;
    return ioctlsocket(sock.raw, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock.raw, F_GETFL, 0);
    if (flags < 0) return false;
    flags = is_blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(sock.raw, F_SETFL, flags) == 0;
#endif
}

FDS_EXT_SOCKET_DEF bool fds_socket_set_timeout(
    FdsSocket sock,
    uint32_t recv_ms,
    uint32_t send_ms) {
    if (!fds_socket_valid(sock)) return false;

    bool ok = true;
#ifdef _WIN32
    DWORD r_to = (DWORD)recv_ms;
    DWORD s_to = (DWORD)send_ms;
    ok = setsockopt(sock.raw, SOL_SOCKET, SO_RCVTIMEO,
                    (const char *)&r_to, sizeof(r_to)) == 0 && ok;
    ok = setsockopt(sock.raw, SOL_SOCKET, SO_SNDTIMEO,
                    (const char *)&s_to, sizeof(s_to)) == 0 && ok;
#else
    struct timeval r_tv;
    struct timeval s_tv;
    r_tv.tv_sec = (time_t)(recv_ms / 1000);
    r_tv.tv_usec = (suseconds_t)((recv_ms % 1000) * 1000);
    s_tv.tv_sec = (time_t)(send_ms / 1000);
    s_tv.tv_usec = (suseconds_t)((send_ms % 1000) * 1000);
    ok = setsockopt(sock.raw, SOL_SOCKET, SO_RCVTIMEO,
                    &r_tv, sizeof(r_tv)) == 0 && ok;
    ok = setsockopt(sock.raw, SOL_SOCKET, SO_SNDTIMEO,
                    &s_tv, sizeof(s_tv)) == 0 && ok;
#endif
    return ok;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_socket_poll(
    FdsSocket sock,
    int timeout_ms,
    bool *can_read,
    bool *can_write) {
    if (!fds_socket_valid(sock) || (!can_read && !can_write)) {
        return FDS_SOCK_RES_ERROR;
    }

#ifdef _WIN32
    fd_set read_fds;
    fd_set write_fds;
    struct timeval tv;
    int res;

    for (;;) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        if (can_read) FD_SET(sock.raw, &read_fds);
        if (can_write) FD_SET(sock.raw, &write_fds);

        struct timeval *tv_ptr = NULL;
        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            tv_ptr = &tv;
        }

        res = select(0,
                     can_read ? &read_fds : NULL,
                     can_write ? &write_fds : NULL,
                     NULL,
                     tv_ptr);

        if (res == SOCKET_ERROR) {
            if (fds_internal_is_interrupted()) continue;
            return FDS_SOCK_RES_ERROR;
        }
        break;
    }
#else
    fd_set read_fds;
    fd_set write_fds;
    struct timeval tv;
    int res;

    for (;;) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        if (can_read) FD_SET(sock.raw, &read_fds);
        if (can_write) FD_SET(sock.raw, &write_fds);

        struct timeval *tv_ptr = NULL;
        if (timeout_ms >= 0) {
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            tv_ptr = &tv;
        }

        res = select(sock.raw + 1,
                     can_read ? &read_fds : NULL,
                     can_write ? &write_fds : NULL,
                     NULL,
                     tv_ptr);

        if (res < 0) {
            if (fds_internal_is_interrupted()) continue;
            return FDS_SOCK_RES_ERROR;
        }
        break;
    }
#endif

    if (res == 0) {
        if (can_read) *can_read = false;
        if (can_write) *can_write = false;
        return FDS_SOCK_RES_WOULD_BLOCK;
    }

    if (can_read) *can_read = FD_ISSET(sock.raw, &read_fds) != 0;
    if (can_write) *can_write = FD_ISSET(sock.raw, &write_fds) != 0;
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_send_raw(
    FdsSocket sock,
    const void *data,
    size_t size) {
    FdsSocketIO io = fds_internal_io_error();
    if (!fds_socket_valid(sock) || !data || size == 0) return io;

    if (size > INT_MAX) size = INT_MAX;

    int r;
    do {
        r = send(sock.raw, FDS_SOCK_BUF(data), (int)size, 0);
    } while (r < 0 && fds_internal_is_interrupted());

    if (r > 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;
    } else if (r == 0) {
        io.result = FDS_SOCK_RES_CLOSED;
    } else {
        io.result = fds_internal_is_would_block() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }
    return io;
}

FDS_EXT_SOCKET_DEF FdsSocketIO fds_socket_recv_raw(
    FdsSocket sock,
    void *buffer,
    size_t size) {
    FdsSocketIO io = fds_internal_io_error();
    if (!fds_socket_valid(sock) || !buffer || size == 0) return io;

    if (size > INT_MAX) size = INT_MAX;

    int r;
    do {
        r = recv(sock.raw, FDS_SOCK_MUT_BUF(buffer), (int)size, 0);
    } while (r < 0 && fds_internal_is_interrupted());

    if (r > 0) {
        io.result = FDS_SOCK_RES_OK;
        io.size = (size_t)r;
    } else if (r == 0) {
        /* recv() == 0 means EOF for TCP. For connected UDP it means an empty
           datagram was received, but callers that need empty UDP datagrams
           should prefer fds_socket_udp_recv_from(). */
        io.result = sock.type == FDS_SOCKET_UDP ? FDS_SOCK_RES_OK : FDS_SOCK_RES_CLOSED;
    } else {
        io.result = fds_internal_is_would_block() ? FDS_SOCK_RES_WOULD_BLOCK : FDS_SOCK_RES_ERROR;
    }
    return io;
}

/* ============================================================================
   Framed TCP stream
   ============================================================================ */
FDS_EXT_SOCKET_DEF void fds_stream_init(
    FdsSocketStream *stream,
    FdsSocket sock,
    uint32_t max_packet_size) {
    if (!stream) return;
    memset(stream, 0, sizeof(*stream));
    stream->socket = sock;
    stream->max_packet_size = max_packet_size > 0
        ? max_packet_size
        : FDS_SOCKET_MAX_PACKET_SIZE;
}

FDS_EXT_SOCKET_DEF void fds_stream_reset_rx(FdsSocketStream *stream) {
    if (!stream) return;
    stream->rx_active = false;
    stream->rx_header_received = 0;
    stream->rx_received = 0;
}

FDS_EXT_SOCKET_DEF void fds_stream_reset_tx(FdsSocketStream *stream) {
    if (!stream) return;
    stream->tx_active = false;
    stream->tx_header_sent = 0;
    stream->tx_sent = 0;
    stream->tx_data = NULL;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_send(
    FdsSocketStream *stream,
    FdsBytesView packet) {
    if (!stream || !fds_socket_valid(stream->socket) ||
        stream->socket.type != FDS_SOCKET_TCP) {
        return FDS_SOCK_RES_ERROR;
    }

    if (packet.size > stream->max_packet_size || packet.size > UINT32_MAX) {
        return FDS_SOCK_RES_ERROR;
    }

    if (!stream->tx_active) {
        stream->tx_active = true;
        stream->tx_packet_size = (uint32_t)packet.size;
        stream->tx_data = packet.data;
        stream->tx_sent = 0;
        stream->tx_header_sent = 0;

        uint32_t le_size = stream->tx_packet_size;
        stream->tx_header[0] = (uint8_t)le_size;
        stream->tx_header[1] = (uint8_t)(le_size >> 8);
        stream->tx_header[2] = (uint8_t)(le_size >> 16);
        stream->tx_header[3] = (uint8_t)(le_size >> 24);
    } else if (stream->tx_data != packet.data ||
               stream->tx_packet_size != (uint32_t)packet.size) {
        /* A pending non-blocking frame may only be continued with the same
           packet memory and size. */
        return FDS_SOCK_RES_ERROR;
    }

    if (stream->tx_header_sent < 4) {
        FdsSocketIO io = fds_socket_send_raw(
            stream->socket,
            stream->tx_header + stream->tx_header_sent,
            4 - stream->tx_header_sent);
        if (io.result != FDS_SOCK_RES_OK) return io.result;
        stream->tx_header_sent += (uint32_t)io.size;
    }

    if (stream->tx_header_sent < 4) return FDS_SOCK_RES_WOULD_BLOCK;

    if (stream->tx_sent < stream->tx_packet_size) {
        FdsSocketIO io = fds_socket_send_raw(
            stream->socket,
            stream->tx_data + stream->tx_sent,
            stream->tx_packet_size - stream->tx_sent);
        if (io.result != FDS_SOCK_RES_OK) return io.result;
        stream->tx_sent += (uint32_t)io.size;
    }

    if (stream->tx_sent < stream->tx_packet_size) return FDS_SOCK_RES_WOULD_BLOCK;

    fds_stream_reset_tx(stream);
    return FDS_SOCK_RES_OK;
}

FDS_EXT_SOCKET_DEF FdsSocketResult fds_stream_recv(
    FdsSocketStream *stream,
    FdsBytesBuilder *out_builder) {
    if (!stream || !out_builder || !fds_socket_valid(stream->socket) ||
        stream->socket.type != FDS_SOCKET_TCP) {
        return FDS_SOCK_RES_ERROR;
    }

    if (!stream->rx_active) {
        if (stream->rx_header_received < 4) {
            FdsSocketIO io = fds_socket_recv_raw(
                stream->socket,
                stream->rx_header + stream->rx_header_received,
                4 - stream->rx_header_received);

            if (io.result != FDS_SOCK_RES_OK) {
                if (io.result == FDS_SOCK_RES_CLOSED && stream->rx_header_received == 0) {
                    return FDS_SOCK_RES_CLOSED;
                }
                if (io.result == FDS_SOCK_RES_CLOSED) {
                    fds_stream_reset_rx(stream);
                    return FDS_SOCK_RES_ERROR;
                }
                return io.result;
            }

            stream->rx_header_received += (uint32_t)io.size;
            if (stream->rx_header_received < 4) return FDS_SOCK_RES_WOULD_BLOCK;
        }

        uint32_t packet_size = ((uint32_t)stream->rx_header[0]) |
                               ((uint32_t)stream->rx_header[1] << 8) |
                               ((uint32_t)stream->rx_header[2] << 16) |
                               ((uint32_t)stream->rx_header[3] << 24);

        if (packet_size > stream->max_packet_size) {
            fds_stream_reset_rx(stream);
            return FDS_SOCK_RES_ERROR;
        }

        stream->rx_packet_size = packet_size;
        stream->rx_active = true;
        stream->rx_received = 0;

        fds_bb_reserve(out_builder, stream->rx_packet_size);
    }

    if (stream->rx_received < stream->rx_packet_size) {
        FdsSocketIO io = fds_socket_recv_raw(
            stream->socket,
            out_builder->data + out_builder->size + stream->rx_received,
            stream->rx_packet_size - stream->rx_received);

        if (io.result != FDS_SOCK_RES_OK) {
            if (io.result == FDS_SOCK_RES_CLOSED) {
                fds_stream_reset_rx(stream);
                return FDS_SOCK_RES_ERROR;
            }
            return io.result;
        }

        stream->rx_received += (uint32_t)io.size;
    }

    if (stream->rx_received < stream->rx_packet_size) return FDS_SOCK_RES_WOULD_BLOCK;

    out_builder->size += stream->rx_packet_size;
    fds_stream_reset_rx(stream);
    return FDS_SOCK_RES_OK;
}

#endif /* FDS_EXT_SOCKET_IMPL */
