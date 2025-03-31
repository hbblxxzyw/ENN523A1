#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>

#define PORT 8888 // Port number for UDP
#define BUFLEN 1024
#define WAITTIME 3000        // 3 seconds wait time (ms)
#define QUITKEY 'E'          // Exit key

// Get current time as string
char* get_time_str() {
    static char timestr[32];
    struct timeval tv;
    struct tm* t;

    gettimeofday(&tv, NULL);  // Get current time
    t = localtime(&tv.tv_sec);

    sprintf(timestr, "%02d:%02d:%02d:%03ld", 
            t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec / 1000);
    return timestr;
}

// Get current time in milliseconds
unsigned long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int main() {
    int sock;
    struct sockaddr_in server, client;
    socklen_t slen = sizeof(client);
    char sendBuf[BUFLEN], recvBuf[BUFLEN];
    int seq = 1;
    unsigned long startTime, endTime;

    printf("======== UDP Server ========\n");

    // Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Set up server address
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr =  INADDR_ANY;
    server.sin_port = htons(PORT);

    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    client.sin_port = htons(9999);


    // Bind socket to address
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    printf("UDP Server running on port %d...\n", PORT);

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    while (1) {
        char seqstr[100];
        sprintf(seqstr, "%d", seq); // Format sequence number

        // Send "R <seq> <timestamp>" to client
        startTime = get_time_ms();
        sprintf(sendBuf, "R %s %s", seqstr, get_time_str());

        if (sendto(sock, sendBuf, sizeof(sendBuf), 0, (struct sockaddr*)&client, sizeof(client)) == -1) {
            perror("Send failed");
            break;
        }
        printf("Sent: %s\n", sendBuf);

        // Receive reply
        int recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&client, &slen);
        if (recvLen == -1) {
            continue;
            //perror("Receive failed");
            //break;
        }
        recvBuf[recvLen] = '\0';
        printf("Received: %s\n", recvBuf);
        endTime = get_time_ms();

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
            if (recvLen == -1) {
                perror("Final ACK receive failed");
            } else {
                recvBuf[recvLen] = '\0';
                printf("Received Final ACK: %s\n", recvBuf);
            }
            break;
        }

        seq++; // Increment sequence number
        usleep(WAITTIME * 1000); // Sleep for 3 seconds
    }

    // Cleanup
    close(sock);
    printf("Server terminated.\n");
    return 0;
}