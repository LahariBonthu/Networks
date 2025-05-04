#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 444
#define FILE_PORT 555
#define BUFFER_SIZE 1024

// Structure to pass multiple arguments to thread
typedef struct {
    int client_sock;
    struct sockaddr_in client_addr;
} client_data_t;

// Function to send a file to the client
void send_file(const char filename[], const char client_ip[]) {
    int file_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (file_sock < 0) {
        perror("File transfer socket creation failed");
        return;
    }

    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(FILE_PORT);
    client_addr.sin_addr.s_addr = inet_addr(client_ip);

    printf("Connecting to client for file transfer on port %d...\n", FILE_PORT);

    if (connect(file_sock, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("File transfer connection failed");
        close(file_sock);
        return;
    }

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        char error_msg[] = "File not found.";
        send(file_sock, error_msg, strlen(error_msg), 0);
        printf("File %s not found.\n", filename);
        close(file_sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(file_sock, buffer, bytes_read, 0);
    }

    printf("File %s  is sent successfully.\n", filename);
    fclose(file);
    close(file_sock); // Close file transfer connection
}

// Function to handle client requests in a separate thread
void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    int client_sock = client_data->client_sock;
    struct sockaddr_in client_addr = client_data->client_addr;
    free(client_data); // Free the allocated memory

    char buffer[BUFFER_SIZE];
    char client_ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        printf("Received: %s\n", buffer);

        if (strncmp(buffer, "Send ", 5) == 0) {
            char *filenames = buffer + 5;
            char *filename = strtok(filenames, ", ");

            while (filename != NULL) {
                send(client_sock, "acknowledge", 3, 0); // Acknowledge file request
                sleep(1); // Give the client time to prepare for file reception
                send_file(filename, client_ip);
                filename = strtok(NULL, ", ");
            }
        } else if (strcmp(buffer, "exit") == 0) {
            break;
        } else {
            send(client_sock, "Unknown command", 15, 0);
        }
    }

    close(client_sock);
    return NULL;
}

// Function to start the multithreaded server
void start_server() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Client accept failed");
            continue;
        }

        printf("Client connected.\n");

        // Allocate memory for client data and create a new thread
        client_data_t *client_data = (client_data_t *)malloc(sizeof(client_data_t));
        client_data->client_sock = client_sock;
        client_data->client_addr = client_addr;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, client_data);
        pthread_detach(thread_id); // Automatically reclaim resources when thread finishes
    }

    close(server_sock);
}

int main() {
    start_server();
    return 0;
}

