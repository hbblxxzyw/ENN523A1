#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>


#define PORT 8888 // Port number for UDP
#define BUFLEN 1024
#define WAITTIME 3000        // 3 seconds wait time (ms)
#define QUITKEY 'E'          // Exit key

/* Function declaration*/
char* get_time_str();
unsigned long get_time_ms();
void *communication_thread(void *args);
/* Struct definition*/
typedef struct {
    int sock;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
} ThreadArgs;

/* Global viarable declaration*/
volatile int exitFlag = 0; // Indicates if exit command received


int main(int argc, char **argv) {
    
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: ./udp_server_linux IPCONFIG_FILENAME\n");
        return 0;
    }

    
    // Initialisation
    int sock;
    pthread_t commThread;
    struct sockaddr_in server, client;
    socklen_t slen = sizeof(client);
    int portNum = 0;
    // Reading ip address and port number from config file
    char ipBuffer[64];
    FILE *ipFile = fopen(argv[1], "r");
    if (!ipFile) {
        perror("The IP configuration file cannot be read. \n");
    }
    fgets(ipBuffer, sizeof(ipBuffer), ipFile);
    ipBuffer[strcspn(ipBuffer, "\n")] = 0; 
    
    fclose(ipFile);
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
    client.sin_addr.s_addr = inet_addr(ipBuffer);
    client.sin_port = htons(9999);


    // Bind socket to address
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("Bind failed");
        close(sock);
        return 1;
    }
    ThreadArgs args;
    args.sock = sock;
    args.serverAddr = server;
    args.clientAddr = client;
    
    pthread_create(&commThread, NULL, communication_thread, &args);
    printf("UDP Server running on port %d...\n", PORT);
    
    while (1) {
        char ch = getchar();
        if (ch == 'E' || ch == 'e') {
            exitFlag = 1;
            break;
        }
    }
    pthread_join(commThread, NULL);

    return 0;
}


void *communication_thread(void *wrappedArgs) {
    
    ThreadArgs *args = (ThreadArgs *)wrappedArgs;
    int sock = args->sock;
    struct sockaddr_in serverAddr = args->serverAddr;
    struct sockaddr_in clientAddr = args->clientAddr;
    socklen_t slen = sizeof(clientAddr);

    char sendBuf[BUFLEN], recvBuf[BUFLEN];
    int seq = 1;
    unsigned long startTime, endTime;

    

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    int recvLen = 0;
    while (!exitFlag) {
        char seqstr[100];
        sprintf(seqstr, "%05d", seq); // Format sequence number

        // Send "R <seq> <timestamp>" to client
        startTime = get_time_ms();
        sprintf(sendBuf, "R %s %s", seqstr, get_time_str());

        if (sendto(sock, sendBuf, sizeof(sendBuf), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
            perror("Send failed");
            break;
        }
        printf("Sent: %s\n", sendBuf);

        // Receive reply
        recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&clientAddr, &slen);
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
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&clientAddr, slen);
            printf("Sent: %s\n", sendBuf);
        }

        

        seq++; // Increment sequence number
        usleep(WAITTIME * 1000); // Sleep for 3 seconds
    }

    // Exit
    char eSeq[6], timeRecv[32];
    int exitSeq = seq++;
    sprintf(sendBuf, "E %05d %s", exitSeq, get_time_str());
    sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&clientAddr, slen);
    printf("Sent Termination ACK: %s\n", sendBuf);

    recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&serverAddr, &slen);
    if (recvLen > 0) {
        recvBuf[recvLen] = '\0';
        if (strncmp(recvBuf, "ACK E", 5) == 0) {
            printf("[Thread] Received: %s\n", recvBuf);

            // Final ACK
            sprintf(sendBuf, "ACK %05d %s", exitSeq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&serverAddr, slen);
            printf("[Thread] Sent final ACK: %s\n", sendBuf);
        } else {
            printf("[Thread] Unexpected reply during exit: %s\n", recvBuf);
        }
    } else {
        printf("[Thread] No ACK E received, proceeding with forced exit.\n");
    }

    // Cleanup
    close(sock);
    printf("Server terminated.\n");
    return NULL;
}

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
