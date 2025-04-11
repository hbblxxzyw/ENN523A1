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
unsigned long get_time_us();
void *communication_thread(void *args);
int get_status(struct sockaddr_in clientAddr);
/* Struct definition*/
typedef struct {
    int sock;
    struct sockaddr_in serverAddr;
    struct sockaddr_in clientAddr;
} ThreadArgs;

/* Global viarable declaration*/
volatile int exitFlag = 0; // Indicates if exit command received
volatile int verboseFlag = 0; // Indicates if verbose mode command received
volatile int connectFlag = 0; // Indicates if a client has connected
volatile unsigned long avgRtt = 0; // Average RTT for latest five communications
volatile int connectDuration = 0;

int main(int argc, char **argv) {
    
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: ./udp_server_linux IPCONFIG_FILENAME\n");
        return 0;
    }

    
    // Initialisation
    int sock;
    pthread_t commThread;
    struct sockaddr_in server, client;

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
        printf("=============== Available Commands ===============\n");
        printf("E / e : Terminate the client-server system\n");
        printf("V / v : Toggle verbose mode (show all sent/received logs)\n");
        printf("S / S : Display connection status\n");
        printf("==================================================\n");
        printf("Please enter your command:\n");
        char ch = getchar();
        getchar(); //remove the enter
        if (ch == 'E' || ch == 'e') {
            exitFlag = 1;
            break;
        }
        if (ch == 'V' || ch == 'v') {
            verboseFlag = !verboseFlag;
        }
        if (ch == 'S' || ch == 's') {
            get_status(client);
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
    long rttRecord[5] = {0};
    int rttCount = 0;
    int rttIndex = 0;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    int recvLen = 0;
    int repliedFlag = 0;
    time_t connectTime = 0;
    
    while (!exitFlag) {
        char seqstr[100];
        repliedFlag = 0;
        sprintf(seqstr, "%05d", seq); // Format sequence number

        // Send "R <seq> <timestamp>" to client
        startTime = get_time_us();

        sprintf(sendBuf, "R %s %s", seqstr, get_time_str());

        if (sendto(sock, sendBuf, sizeof(sendBuf), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
            perror("Send failed");
            break;
        }
        if (verboseFlag) {
            printf("Sent: %s\n", sendBuf);
        }
        

        // Receive reply
        recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&clientAddr, &slen);
        if (recvLen == -1) {
            continue;
            //perror("Receive failed");
            //break;
        }
        recvBuf[recvLen] = '\0';
        if (verboseFlag) {
            printf("Received: %s\n", recvBuf);
        }
        endTime = get_time_us();

        // Handle ACK R
        if (strncmp(recvBuf, "ACK R", 5) == 0) {
            repliedFlag = 1;
            rttRecord[rttIndex] = endTime - startTime;
            rttIndex = (rttIndex + 1) % 5;
            if (rttCount < 5) {
                rttCount++;
            } 
            unsigned long total = 0;
            for (int i = 0; i < rttCount; i++) {
                total += rttRecord[i];
            }
            avgRtt = total / rttCount;
            if (verboseFlag) {
                printf("RTT for seq %s = %lu us\n", seqstr, endTime - startTime);
            }
            sprintf(sendBuf, "ACK %s %s", seqstr, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&clientAddr, slen);
            if (verboseFlag) {
                printf("Sent: %s\n", sendBuf);
            }
        }
        if (repliedFlag && !connectFlag) {
            // record the time of connection
            connectTime = time(NULL);
            connectFlag = 1;
        } else if (!repliedFlag) {
            connectFlag = 0;
            connectTime = 0;
        } 
        
        if (connectTime) {
            time_t now = time(NULL);
            connectDuration = (int)difftime(now, connectTime);
        }
        

        seq++; // Increment sequence number
        usleep(WAITTIME * 1000); // Sleep for 3 seconds
    }

    // Exit
    int exitSeq = seq++;
    sprintf(sendBuf, "E %05d %s", exitSeq, get_time_str());
    sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&clientAddr, slen);
    printf("Sent Termination ACK: %s\n", sendBuf);


    recvLen = recvfrom(sock, recvBuf, BUFLEN, 0, (struct sockaddr*)&serverAddr, &slen);
    if (recvLen > 0) {
        recvBuf[recvLen] = '\0';
        if (strncmp(recvBuf, "ACK E", 5) == 0) {
            if (verboseFlag) {
                printf("Received: %s\n", recvBuf);
            }
            // Final ACK
            sprintf(sendBuf, "ACK %05d %s", exitSeq, get_time_str());
            sendto(sock, sendBuf, strlen(sendBuf), 0, (struct sockaddr*)&serverAddr, slen);
            if (verboseFlag) {
                printf("Sent final ACK: %s\n", sendBuf);
            }
        } else {

            printf("Unexpected reply during exit: %s\n", recvBuf);
        }
    } else {
        printf("No ACK E received, proceeding with forced exit.\n");
    }

    // Cleanup
    close(sock);
    printf("Server terminated.\n");
    return NULL;
}

int get_status(struct sockaddr_in clientAddr) {
    printf("\n=============== Server Status ====================\n");
    if (!connectFlag) {
        printf("Client connection status : Not connected\n");
    } else {
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIp, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);
        int hours = connectDuration / 3600;
        int minutes = (connectDuration % 3600) / 60;
        int seconds = connectDuration % 60;

        printf("Client connection status : Connected\n");
        printf("Client IP   : %s\n", clientIp);
        printf("Client Port : %d\n", clientPort);
        printf("Connected for : %02d:%02d:%02d\n", hours, minutes, seconds);
        }
    printf("==================================================\n");
    return 0;
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

// Get current time in microseconds
unsigned long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000) + (tv.tv_usec);
}