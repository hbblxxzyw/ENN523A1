
// ENN523A1 - UDP Client (Linux Version)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_IP "127.0.0.1" // local testing IP
#define PORT 9999            // same port as server
#define BUFLEN 1024
#define QUITKEY 'E'

// Get current timestamp string (similar to Windows version)
char* get_time_str() {
    static char timestr[32];
    struct timespec ts;
    struct tm *tm_info;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    tm_info = localtime(&ts.tv_sec);
    
    snprintf(timestr, sizeof(timestr), "%02d:%02d:%02d:%03ld", 
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ts.tv_nsec / 1000000);
    return timestr;
}

int main() {
    int sock;
    struct sockaddr_in server;
    socklen_t slen = sizeof(server);
    char sendBuf[BUFLEN], recvBuf[BUFLEN];

    printf("======== UDP Client ========\n");

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(sock, (const struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("UDP Client running. Waiting for messages...\n");



    while (1) {
        // Wait for message from server
        int recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&server, &slen);
        if (recvLen == -1) {
            perror("recvfrom() failed");
            break;
        }

        recvBuf[recvLen] = '\0';
        printf("Received: %s\n", recvBuf);

        // Parse and respond
        char seq[6], timestamp[32];
        sscanf(recvBuf, "%*s %s %s", seq, timestamp);

        // Check message type
        if (recvBuf[0] == 'R') {
            // Respond with: ACK R <seq> <timestamp>
            snprintf(sendBuf, sizeof(sendBuf), "ACK R %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server, slen);
            printf("Sent: %s\n", sendBuf);
        }
        // Exit
        else if (recvBuf[0] == QUITKEY) {
            // Received termination message: E <seq> <timestamp>
            snprintf(sendBuf, sizeof(sendBuf), "ACK E %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server, slen);
            printf("Sent Termination ACK: %s\n", sendBuf);

            // Final ACK from server
            recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&server, &slen);
            if (recvLen == -1) {
                perror("Final ACK receive failed");
            } else {
                recvBuf[recvLen] = '\0';
                printf("Received Final ACK: %s\n", recvBuf);
            }
            break;
        }
    }

    // Cleanup
    close(sock);
    printf("Client terminated.\n");
    return 0;
}