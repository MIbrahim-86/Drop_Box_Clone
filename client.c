#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

int connect_to_server() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    printf("Attempting to connect to server at %s:%d...\n", SERVER_IP, SERVER_PORT);
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error: %s\n", strerror(errno));
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        close(sock);
        return -1;
    }
    
    printf("Connecting to server...\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed: %s\n", strerror(errno));
        printf("Make sure the server is running with: ./dropbox_server\n");
        close(sock);
        return -1;
    }
    
    printf("Successfully connected to server!\n");
    return sock;
}

int main() {
    int sock = connect_to_server();
    if (sock < 0) {
        printf("Press Enter to exit...");
        getchar();
        return 1;
    }
    
    char buffer[BUFFER_SIZE];
    printf("Connected to server. Commands: SIGNUP, LOGIN, UPLOAD, DOWNLOAD, DELETE, LIST, QUIT\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            printf("\nEOF detected. Closing connection...\n");
            break;
        }
        
        buffer[strcspn(buffer, "\n")] = 0;
        
        
        if (strlen(buffer) == 0) {
            continue;
        }
        
        strcat(buffer, "\n");
        
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            printf("Send failed: %s\n", strerror(errno));
            break;
        }
        
        if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("Quitting...\n");
            break;
        }
        
        printf("Server response:\n");
        memset(buffer, 0, sizeof(buffer));
        
        int total_received = 0;
        int bytes_received;
        
        struct timeval tv;
        tv.tv_sec = 5;  
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
        
        while ((bytes_received = recv(sock, buffer + total_received, 
                                    sizeof(buffer) - total_received - 1, 0)) > 0) {
            total_received += bytes_received;
            buffer[total_received] = '\0';
            
            if (strstr(buffer, "END_FILE") != NULL ||
                strstr(buffer, "OK:") != NULL ||
                strstr(buffer, "ERROR:") != NULL ||
                strstr(buffer, "Files:") != NULL ||
                total_received >= sizeof(buffer) - 1) {
                break;
            }
        }
        
        if (bytes_received < 0) {
            printf("Receive timeout or error\n");
        }
        
        printf("%s\n", buffer);
    }
    
    close(sock);
    printf("Disconnected from server\n");
    return 0;
}

