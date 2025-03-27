// server/udp_server_simple.c
// ENN523A1 - UDP Server

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 8888            // port number for UDP
#define BUFLEN 1024
#define WAITTIME 3000        // 3 seconds wait time
#define QUITKEY 'E'          // exit key

// Get current time as string
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
    struct sockaddr_in server, client;
    int slen = sizeof(client);
    char sendBuf[BUFLEN], recvBuf[BUFLEN];
    int seq = 1;
    DWORD startTime, endTime;

    printf("======== UDP Server ========\n");

    // Start Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup failed: %d\n", WSAGetLastError());
        return 1;
    }

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Set up server address
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("UDP Server running on port %d...\n", PORT);

    while (1) {
        char seqstr[6];
        sprintf(seqstr, "%05d", seq); // Format sequence number

        // Send "R <seq> <timestamp>" to client
        startTime = GetTickCount();
        sprintf(sendBuf, "R %s %s", seqstr, get_time_str());

        if (sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&client, slen) == SOCKET_ERROR) {
            printf("Send failed: %d\n", WSAGetLastError());
            break;
        }
        printf("Sent: %s\n", sendBuf);

        // Receive reply
        int recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&client, &slen);
        if (recvLen == SOCKET_ERROR) {
            printf("Receive failed: %d\n", WSAGetLastError());
            break;
        }
        recvBuf[recvLen] = '\0';
        printf("Received: %s\n", recvBuf);
        endTime = GetTickCount();

        // Handle ACK R
        if (strncmp(recvBuf, "ACK R", 5) == 0) {
            printf("RTT for seq %s = %lu ms\n", seqstr, endTime - startTime);
            sprintf(sendBuf, "ACK %s %s", seqstr, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&client, slen);
            printf("Sent: %s\n", sendBuf);
        }

        // Exit
        else if (recvBuf[0] == QUITKEY) {
            char eSeq[6], timeRecv[32];
            sscanf(recvBuf, "%*s %s %s", eSeq, timeRecv);
            sprintf(sendBuf, "ACK E %s %s", eSeq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&client, slen);
            printf("Sent Termination ACK: %s\n", sendBuf);

            // Final ACK from client
            recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&client, &slen);
            if (recvLen == SOCKET_ERROR) {
                printf("Final ACK receive failed: %d\n", WSAGetLastError());
            } else {
                recvBuf[recvLen] = '\0';
                printf("Received Final ACK: %s\n", recvBuf);
            }
            break;
        }

        seq++; // Add sequence number
        Sleep(WAITTIME); // Wait 3 seconds
    }

    // Cleanup
    closesocket(sock);
    WSACleanup();
    printf("Server terminated.\n");
    return 0;
}
