// client/udp_client_simple.c
// ENN523A1 - UDP Client

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1" // local testing IP
#define PORT 8888             // same port as server
#define BUFLEN 1024
#define QUITKEY 'E'

// get current timestamp string (same as server)
char* get_time_str() {
    static char timestr[32];
    SYSTEMTIME t;
    GetLocalTime(&t);
    sprintf(timestr, "%02d:%02d:%02d:%03d", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return timestr;
}

int main() {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    int slen = sizeof(server);
    char sendBuf[BUFLEN], recvBuf[BUFLEN];

    printf("======== UDP Client ========\n");

    // Start Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        exit(SOCKET_ERROR)
    }

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        exit(INVALID_SOCKET)
    }

    // Set up server address
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(PORT);

    printf("UDP Client running. Waiting for messages...\n");

    while (1) {
        // Wait for message from server
        int recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&server, &slen);
        if (recvLen == SOCKET_ERROR) {
            printf("recvfrom() failed: %d\n", WSAGetLastError());
            break;
        }

        recvBuf[recvLen] = '\0';
        printf("Received: %s\n", recvBuf);

        // Parse and respond
        char seq[6], timestamp[32];
        sscanf(recvBuf, "%*s %s %s", seq, timestamp);

        // Check message type
        // Request 
        if (recvBuf[0] == 'R') {
            // Respond with: ACK R <seq> <timestamp>
            sprintf(sendBuf, "ACK R %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server, slen);
            printf("Sent: %s\n", sendBuf);
        }
        // Exit
        else if (recvBuf[0] == QUITKEY) {
            // Received termination message: E <seq> <timestamp>
            sprintf(sendBuf, "ACK E %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server, slen);
            printf("Sent Termination ACK: %s\n", sendBuf);

            // Final ACK from server
            recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&server, &slen);
            if (recvLen == SOCKET_ERROR) {
                printf("Final ACK receive failed: %d\n", WSAGetLastError());
            } else {
                recvBuf[recvLen] = '\0';
                printf("Received Final ACK: %s\n", recvBuf);
            }
            break;
        }
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    printf("Client terminated.\n");
    return 0;
}
