#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"
#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#define FDS_EXT_SOCKET_IMPL
#include "fds_ext_socket.h"

int main(void) {
    if (!fds_net_init()) {
        printf("Failed to initialize network.\n");
        return 1;
    }

    FdsSocket sock = fds_socket_connect("127.0.0.1", 8080);
    if (!fds_socket_valid(sock)) {
        printf("Failed to connect to server at 127.0.0.1:8080.\n");
        fds_net_cleanup();
        return 1;
    }

    printf("Connected to server successfully!\n");

    FdsSocketStream stream;
    fds_stream_init(&stream, sock, 1024 * 1024);

    // Надсилаємо текстове повідомлення
    const char *message = "Hello FDS Server, this is a test message from client!";
    FdsBytesView packet = { .data = (const uint8_t *)message, .size = strlen(message) };

    FdsSocketResult r = fds_stream_send(&stream, packet);
    if (r != FDS_SOCK_RES_OK) {
        printf("Failed to send message to server.\n");
    } else {
        printf("Message sent: %s\n", message);
    }

    // Чекаємо відповідь від сервера
    FdsBytesBuilder builder = {0};
    while (true) {
        r = fds_stream_recv(&stream, &builder);
        if (r == FDS_SOCK_RES_OK) {
            char *reply_ptr = (char *)(builder.data + builder.size - stream.rx_packet_size);
            printf("Server response: %.*s\n", (int)stream.rx_packet_size, reply_ptr);
            break;
        } else if (r == FDS_SOCK_RES_ERROR || r == FDS_SOCK_RES_CLOSED) {
            printf("Connection lost while waiting for response.\n");
            break;
        }
    }

    fds_socket_close(&sock);
    fds_net_cleanup();

    printf("Client finished.\n");
    return 0;
}