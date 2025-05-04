#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define PORT 444
#define FILE_PORT 555
#define BUFFER_SIZE 1024

// Function to receive a file from the server
void receive_file(const char filename[]) {
    int file_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (file_sock < 0) {
        perror("File reception socket creation failed");
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FILE_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(file_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed for file reception");
        close(file_sock);
        return;
    }

    if (listen(file_sock, 1) < 0) {
        perror("Listen failed for file reception");
        close(file_sock);
        return;
    }

    printf("Waiting for file transfer: %s...\n", filename);
    int file_conn = accept(file_sock, NULL, NULL);
    if (file_conn < 0) {
        perror("Accept failed for file reception");
        close(file_sock);
        return;
    }

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(file_conn, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Error receiving data from server.\n");
        close(file_conn);
        close(file_sock);
        return;
    }

    buffer[bytes_received] = '\0'; // Null terminate for string operations

    // Check if server sent "File not found." before creating a file
    if (strncmp(buffer, "File not found.", 15) == 0) {
        printf("Error: %s\n", buffer); // Print error and do NOT create a file
        close(file_conn);
        close(file_sock);
        return;
    }

    // Only create a file if actual data is received
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open file to write");
        close(file_conn);
        close(file_sock);
        return;
    }

    fwrite(buffer, 1, bytes_received, file);

    // Continue receiving and writing remaining data
    while ((bytes_received = recv(file_conn, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
    }

    printf("File received successfully: %s\n", filename);
    fclose(file);
    close(file_conn);
    close(file_sock);
}


// Function to start the client
void start_client() {
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        char message[BUFFER_SIZE];
        printf("Enter message or file request (e.g., Send file.txt): ");
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "exit") == 0) {
            send(client_sock, "exit", 4, 0);
            break;
        }

        send(client_sock, message, strlen(message), 0);

        if (strncmp(message, "Send ", 5) == 0) {
            char *filenames = message + 5;
            char *filename = strtok(filenames, ", ");
            while (filename != NULL) {
                char response[BUFFER_SIZE] = {0};
                recv(client_sock, response, sizeof(response), 0);
                printf("Server response: %s\n", response);
                receive_file(filename);
                filename = strtok(NULL, ", ");
            }
        }
    }

    close(client_sock);
}

int main() {
    start_client();
    return 0;
}
