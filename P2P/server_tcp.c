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

void handle_client(int client_sock) {
	char buffer[1024] = {0};
	
	while(1){
	memset(buffer,0,1024);
    	//read(client_sock, buffer, sizeof(buffer));
    	printf("waiting to recieve data...\n");
    	recv(client_sock,buffer,sizeof(buffer),0);
    	printf("Received: %s\n", buffer);
	if (strncmp(buffer, "Send ", 5) == 0) {
		char *file_request = buffer + 5;  // Skip "Send "
		
		// Split filenames by commas
		char *token = strtok(file_request, ",");
		char response[1024] = {0};

		    memset(response,0,1024);
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
                    strcat(response,"\n");
                    
		    
		    token = strtok(NULL, ",");
		}
		    send(client_sock, response, strlen(response), 0);
		  printf("%s",response);

		// Send all IP addresses or not found messages in one go
	    }
	    }

   	 close(client_sock);    	
}

int main() {
	int server_fd,client_sock;
	struct sockaddr_in server_addr,client_addr;
	socklen_t addr_len = sizeof(client_addr);
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
    	if (server_fd == -1) {
        	perror("Socket creation failed");
        	exit(EXIT_FAILURE);
    	}
    	int opt=1;
    	setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    	server_addr.sin_family = AF_INET;
    	//server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
    	server_addr.sin_port = htons(PORT);	
    	 inet_pton(AF_INET,SERVER_IP	, &server_addr.sin_addr); 
    	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        	perror("Bind failed");
        	exit(EXIT_FAILURE);
    	}
    	listen(server_fd,10);
    	printf("Server listening on port %d...\n", PORT);
    	
        while(1) {
    	client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            return 0;
        }
        if(fork()==0){
        	printf("Connection from %s\n", inet_ntoa(client_addr.sin_addr));
        	handle_client(client_sock);
        	exit(0);
        }
        
        }
    	//close(client_sock);
    	close(server_fd);
    	return 0;
}
