#include <stdio.h>

#define FDS_IMPLEMENTATION
#include "fds.h"
#define FDS_EXT_IMPL
#include "fds_ext.h"
#define FDS_EXT_BYTES_IMPL
#include "fds_ext_bytes.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 6969

int main()
{

    WSADATA wsaData;
    SOCKET serverSocket = INVALID_SOCKET;

    SOCKET clientSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);

    const char *httpResponse = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: 23\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello from Win32 Socket!";

    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed with error: %d\n", result);
        return 1;
    }
    printf("Winsock initialized successfully.\n");

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        printf("Socket creation failed with error: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;


    result = bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        printf("Bind failed with error: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    result = listen(serverSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        printf("Listen failed with error: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }
    printf("Server is listening on port %d... Waiting for connections...\n", PORT);

    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
    if (clientSocket == INVALID_SOCKET) {
        printf("Accept failed with error: %d\n", WSAGetLastError());
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    int bytesSent = send(clientSocket, httpResponse, (int)strlen(httpResponse), 0);
    if (bytesSent == SOCKET_ERROR) {
            printf("Send failed with error: %d\n", WSAGetLastError());
        } else {
            printf("Successfully sent %d bytes to the client.\n", bytesSent);
        }

    // Перетворюємо IP-адресу клієнта у читаємий вигляд
    // char clientIP[INET_ADDRSTRLEN];
    // getnameinfo((struct sockaddr*)&clientAddr, clientAddrLen, clientIP, sizeof(clientIP), NULL, 0, NI_NUMERICHOST);
    // printf("Client connected from IP: %s, Port: %d\n", clientIP, ntohs(clientAddr.sin_port));


    closesocket(serverSocket);
    WSACleanup();
    // socket
    return 0;
}