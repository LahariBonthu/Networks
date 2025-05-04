#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 4444
#define SERVER_IP "192.168.2.3"  

struct FileDB {
    char filename[50];
    char ip_address[20];
};

struct FileDB files[] = {
    {"file1.txt", "192.168.2.5"},
    {"file2.txt", "192.168.2.6"},
    {"file3.txt", "127.0.0.2"},
    {"file1.txt", "127.0.0.4"},
    {"file1.txt", "127.0.0.7"},
    {"file1.txt", "127.0.0.5"},
    {"file1.txt", "127.0.0.6"},
    {"file2.txt", "192.168.2.5"},
};
int file_count = 8;

void handle_client(int sock, struct sockaddr_in *client_addr, socklen_t client_len, char *buffer) {
    printf("waiting to receive data...\n");
    printf("Received: %s\n", buffer);
    
    if (strncmp(buffer, "Send ", 5) == 0) {
        char *file_request = buffer + 5;  // Skip "Send "
        
        // Split filenames by commas
        char *token = strtok(file_request, ",");
        char response[1024] = {0};
        
        memset(response, 0, 1024);
        while (token != NULL) {
            char filename[50];
            strcpy(filename, token);
            filename[strcspn(filename, "\n")] = 0;  // Remove trailing newline

            int found = 0;
            strcat(response, filename);
            strcat(response, " -> ");
            for (int i = 0; i < file_count; i++) {
                if (strcmp(files[i].filename, filename) == 0) {
                    strcat(response, files[i].ip_address);
                    strcat(response, ", ");
                    found = 1;    
                }
            }

            if (!found) {
                strcat(response, filename);
                strcat(response, " -> File not found");
            }
            strcat(response, "\n");
            
            token = strtok(NULL, ",");
        }
        
        sendto(sock, response, strlen(response), 0, (struct sockaddr *)client_addr, client_len);
        printf("%s", response);
    }
}

int main() {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[1024];
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", PORT);
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &client_len);
        if(fork()==0){
        if (n > 0) {
            buffer[n] = '\0';
            handle_client(sock, &client_addr, client_len, buffer);
        }
        }
    }
    
    close(sock);
    return 0;
}
