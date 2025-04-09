// ENN523A1 - UDP Client (Linux Version)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define SERVER_IP "127.0.0.1" // Server IP
#define SERVER_PORT 8888      // Server listening port
#define CLIENT_PORT 9999      // Client binds to this port
#define BUFLEN 1024
#define QUITKEY 'E'

// Get current timestamp string (hh:mm:ss:SSS)
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

int main(int argc, char **argv) {

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: ./udp_client_linux IPCONFIG_FILENAME\n");
        return 0;
    }
    int sock;
    struct sockaddr_in client_addr, server_addr;
    socklen_t slen = sizeof(server_addr);
    char sendBuf[BUFLEN], recvBuf[BUFLEN];

    char ipBuffer[64];
    FILE *ipFile = fopen(argv[1], "r");
    if (!ipFile) {
        perror("The IP configuration file cannot be read. \n");
    }
    fgets(ipBuffer, sizeof(ipBuffer), ipFile);
    ipBuffer[strcspn(ipBuffer, "\n")] = 0; 
    
    fclose(ipFile);

    printf("======== UDP Client ========\n");

    // Create UDP socket
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Bind to client port
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(CLIENT_PORT);

    if (bind(sock, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Set up server address (for sendto)
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, ipBuffer, &server_addr.sin_addr);

    printf("UDP Client running on port %d. Waiting for messages...\n", CLIENT_PORT);

    while (1) {
        // Wait for message from server
        int recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, NULL, NULL);
        if (recvLen == -1) {
            perror("recvfrom() failed");
            break;
        }

        recvBuf[recvLen] = '\0';
        printf("Received: %s\n", recvBuf);

        char type[8] = {0}, seq[8] = {0}, timestamp[32] = {0};
        sscanf(recvBuf, "%s %s %s", type, seq, timestamp);

        if (strcmp(type, "R") == 0 || strcmp(type, "r") == 0) {
            // Respond to request
            snprintf(sendBuf, sizeof(sendBuf), "ACK R %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server_addr, slen);
            printf("Sent: %s\n", sendBuf);
        }
        else if (strcmp(type, "E") == 0 || strcmp(type, "e") == 0) {
            // First ACK: ACK E <seq> <client_time>
            snprintf(sendBuf, sizeof(sendBuf), "ACK E %s %s", seq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&server_addr, slen);
            printf("Sent Termination ACK: %s\n", sendBuf);

            // Wait for final ACK from server: ACK <seq> <timestamp>
            recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, NULL, NULL);
            if (recvLen == -1) {
                perror("Final ACK receive failed");
                printf("⚠️ No final ACK from server. Proceeding to terminate anyway.\n");
            } else {
                recvBuf[recvLen] = '\0';
                printf("Received Final ACK: %s\n", recvBuf);
            }

            break;
        }
    }

    close(sock);
    printf("Client terminated.\n");
    return 0;
}