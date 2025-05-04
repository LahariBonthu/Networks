#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#define PORT 4444
#define SERVER_IP "192.168.2.3"  // Server IP
#define MY_IP "192.168.2.6"
#define FILEPORT 5555
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5

int receive_file(char* ip, char* filenm);
void *client_server_handler(void *arg);
void *handle_client(void *arg);
void *process_file_ipdata(void *arg);
struct client_info {
    int socket;
    struct sockaddr_in addr;
    socklen_t addr_len;
    char buffer[BUFFER_SIZE];
};

typedef struct {
    char line[1024];
} data;

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024] = {0};
    
    // Start server thread to listen for other clients
    pthread_t server_thread;
    if (pthread_create(&server_thread, NULL, client_server_handler, NULL) != 0) {
        perror("Failed to create server thread");
        exit(EXIT_FAILURE);
    }
    
    // Create socket (UDP)
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    printf("trying to connect (UDP)...\n");
    
    while (1) {
        printf("Enter filenames (comma-separated): \n");
        fflush(stdout);
        
        char filenames[100];
        memset(filenames, 0, 100);
        fgets(filenames, sizeof(filenames), stdin);
        
        sendto(sock, filenames, strlen(filenames), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from_addr, &from_len);
        
        if (bytes > 0) {
	    int ipdet_fd = open("ipdetails.txt", O_WRONLY | O_CREAT | O_APPEND, 0666);
	    if (ipdet_fd < 0) {
		perror("File open failed");
		close(sock);
		return 0;
	    }
            buffer[bytes] = '\0';
            printf("%s", buffer);
            fflush(stdout);
            write(ipdet_fd, buffer, bytes);
            close(ipdet_fd);
        }
   
        char *line, *saveptr1;
        line = strtok_r(buffer, "\n", &saveptr1);
        
        while (line != NULL) {
            data *args = (data *)malloc(sizeof(data));
            strncpy(args->line, line, sizeof(args->line) - 1);

            pthread_t line_thread;
            if (pthread_create(&line_thread, NULL, process_file_ipdata, (void *)args) != 0) {
                perror("Failed to create thread for line processing");
                free(args);
            }
            pthread_detach(line_thread);

            line = strtok_r(NULL, "\n", &saveptr1);
        }
        printf("hi\n");
        fflush(stdout);
    }
    
    close(sock);
    return 0;
}

void *client_server_handler(void *arg) {
    int sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Create UDP server socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Server socket creation failed");
        pthread_exit(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FILEPORT);
    inet_pton(AF_INET, MY_IP, &server_addr.sin_addr);
    
    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        pthread_exit(NULL);
    }

    printf("\nListening for other clients on port %d (UDP)...\n", FILEPORT);

    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        struct client_info *cinfo = malloc(sizeof(struct client_info));
            memset(cinfo, 0, sizeof(struct client_info));
        cinfo->socket = sock;
        cinfo->addr_len = sizeof(cinfo->addr);
        
        int bytes = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cinfo->addr, &cinfo->addr_len);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            printf("Received from client: %s\n", buffer);
            pthread_t client_thread;
            strcpy(cinfo->buffer,buffer);
            if (pthread_create(&client_thread, NULL, handle_client, (void *)cinfo) != 0) {
                perror("Failed to create client thread");
                free(cinfo);
            }
            pthread_detach(client_thread);
        } else {
            free(cinfo);
        }
    }
    
    close(sock);
    pthread_exit(NULL);
}

//*********************************************************************************************************************************************************************
int receive_file(char* ip, char* filenm) {
    int sock;
    struct sockaddr_in file_addr;
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("File socket creation failed");
        return 0;
    }
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    file_addr.sin_family = AF_INET;
    file_addr.sin_port = htons(FILEPORT);
    inet_pton(AF_INET, ip, &file_addr.sin_addr);
    
    //printf("connected to: %s\n", ip);
    printf("sending filename: %s\n", filenm);
    
    sendto(sock, filenm, strlen(filenm), 0, (struct sockaddr *)&file_addr, sizeof(file_addr));
    
    int file_fd = open(filenm, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
        perror("File open failed");
        close(sock);
        return 0;
    }
    
    ssize_t bytes_received;
    while ((bytes_received = recvfrom(sock, buffer, BUFFER_SIZE, 0, NULL, NULL)) > 0) {
    
        write(file_fd, buffer, bytes_received);
        printf("recieved %zu bytes of data...\n",bytes_received);
    }
    
     if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout: No response received.\n");
        } else {
            perror("recvfrom failed");
        }
        close(file_fd);
        close(sock);
        return 0;
    }
    
    close(file_fd);
    close(sock);
    return 1;
}


void *handle_client(void *arg) {
    struct client_info *cinfo = (struct client_info *)arg;
    char buffer[BUFFER_SIZE] = {0};
    strcpy(buffer,cinfo->buffer);
    printf("Handling client from %s:%d\n",
           inet_ntoa(cinfo->addr.sin_addr), ntohs(cinfo->addr.sin_port));
    
    //int bytes = recvfrom(cinfo->socket, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cinfo->addr, &cinfo->addr_len);
    printf("received in handle client: %s\n", buffer);
    
    int file_fd = open(buffer, O_RDONLY);
    printf("opening the file...\n");
    if (file_fd < 0) {
        perror("File open failed\n");
    } else {
        ssize_t bytes_read;
        while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
            printf("sending %zu bytes of data...\n",bytes_read);
            sendto(cinfo->socket, buffer, bytes_read, 0, (struct sockaddr *)&cinfo->addr, cinfo->addr_len);
        }
        printf("sending %zu bytes of data...\n",bytes_read);
        sendto(cinfo->socket, buffer, bytes_read, 0, (struct sockaddr *)&cinfo->addr, cinfo->addr_len);
        close(file_fd);
    }
    free(cinfo);
    pthread_exit(NULL);
}

void *process_file_ipdata(void *arg) {
    data *args = (data *)arg;
    char *line = args->line;
    char *arrow, *ip_token;
    char *saveptr2;
    
    printf("\nThread processing line: %s\n", line);

    arrow = strstr(line, "-> ");
    char filenm[20] = {0};
    
    if (arrow) {
        strncpy(filenm, line, arrow - line - 1);
        arrow += 3;
        ip_token = strtok_r(arrow, ", ", &saveptr2);
        
        while (ip_token != NULL) {
            printf("IP: %s\n", ip_token);
            int result = receive_file(ip_token, filenm);
            if (result) {
                printf("result: %d for filename: %s\n", result, filenm);
                printf("File %s received successfully.\n", filenm);
                break;
            }
            ip_token = strtok_r(NULL, ", ", &saveptr2);
        }
    }
    
    free(args);
    pthread_exit(NULL);
}
