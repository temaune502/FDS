#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"
#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#define FDS_EXT_SOCKET_IMPL
#include "fds_ext_socket.h"

#define PORT 6969

int main2(void) {
    if (!fds_net_init()) {
        printf("Failed to initialize network.\n");
        return 1;
    }

    FdsSocket server = fds_socket_listen("127.0.0.1", 8080, 5);
    if (!fds_socket_valid(server)) {
        printf("Failed to bind/listen on port 8080.\n");
        fds_net_cleanup();
        return 1;
    }

    printf("Server listening on 127.0.0.1:8080...\n");

    FdsSocket client;
    char client_ip[64];
    FdsSocketResult res = fds_socket_accept(server, &client, client_ip, sizeof(client_ip));
    if (res != FDS_SOCK_RES_OK) {
        printf("Failed to accept client connection.\n");
        fds_socket_close(&server);
        fds_net_cleanup();
        return 1;
    }

    printf("Client connected from: %s\n", client_ip);

    FdsSocketStream stream;
    fds_stream_init(&stream, client, 1024 * 1024);

    FdsBytesBuilder builder = {0};

    // Очікування повідомлення від клієнта
    while (true) {
        FdsSocketResult r = fds_stream_recv(&stream, &builder);
        if (r == FDS_SOCK_RES_OK) {
            // Виводимо отриманий текст (безпечно через довжину пакета)
            char *text_ptr = (char *)(builder.data + builder.size - stream.rx_packet_size);
            printf("Received from client: %.*s\n", (int)stream.rx_packet_size, text_ptr);

            // Надсилаємо відповідь
            const char *reply = "Hello from FDS Server! Message received.";
            FdsBytesView reply_view = { .data = (const uint8_t *)reply, .size = strlen(reply) };
            
            fds_stream_send(&stream, reply_view);
            break;
        } else if (r == FDS_SOCK_RES_ERROR || r == FDS_SOCK_RES_CLOSED) {
            printf("Connection closed or error occurred.\n");
            break;
        }
    }

    // Очищення
    if (builder.data) {
        // Якщо у тебе використовується fds_bb_free, звільни буфер тут
    }
    fds_socket_close(&client);
    fds_socket_close(&server);
    fds_net_cleanup();

    printf("Server shut down cleanly.\n");
    return 0;
}




//////////
int main(void) {
    if (!fds_net_init()) {
        printf("Failed to initialize network.\n");
        return 1;
    }

    // Підключаємося до google.com через port 80 (HTTP)
    // Завдяки getaddrinfo() ім'я хоста автоматично резолвиться в IP
    const char *host = "google.com";
    uint16_t port = 80;

    printf("Connecting to %s:%u...\n", host, port);
    FdsSocket sock = fds_socket_connect(host, port);
    if (!fds_socket_valid(sock)) {
        printf("Failed to connect to %s\n", host);
        fds_net_cleanup();
        return 1;
    }

    printf("Connected successfully! Sending HTTP request...\n");

    // Формуємо простий HTTP GET запит
    const char *http_request = 
        "GET / HTTP/1.1\r\n"
        "Host: google.com\r\n"
        "Connection: close\r\n\r\n";

    // Відправляємо сирі байти
    FdsSocketIO send_res = fds_socket_send_raw(sock, http_request, strlen(http_request));
    if (send_res.result != FDS_SOCK_RES_OK) {
        printf("Failed to send HTTP request.\n");
        fds_socket_close(&sock);
        fds_net_cleanup();
        return 1;
    }

    printf("Response from server:\n----------------------------------------\n");

    // Читаємо відповідь у циклиі за допомогою raw API, доки сервер не закриє з'єднання (EOF)
    char buffer[4096];
    while (true) {
        FdsSocketIO recv_res = fds_socket_recv_raw(sock, buffer, sizeof(buffer) - 1);
        
        if (recv_res.result == FDS_SOCK_RES_OK) {
            buffer[recv_res.size] = '\0';
            printf("%s", buffer);
        } 
        else if (recv_res.result == FDS_SOCK_RES_CLOSED) {
            // Сервер завершив передачу даних і закрив з'єднання (стандарт для HTTP/1.0 та Connection: close)
            break;
        } 
        else {
            printf("\n[Error] Receive failed.\n");
            break;
        }
    }

    printf("\n----------------------------------------\n");

    fds_socket_close(&sock);
    fds_net_cleanup();
    return 0;
}