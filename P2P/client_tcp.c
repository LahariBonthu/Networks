#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <fcntl.h>
#define PORT 4444
#define SERVER_IP "192.168.2.3"  // Server IP
#define MY_IP "192.168.2.6"
#define FILEPORT 5555
#define BUFFER_SIZE 1024

int receive_file(char* ip,char* filenm);
void *client_server_handler(void *arg);
void *handle_client(void *arg);
void *process_file_ipdata(void * arg);
struct client_info {
    int socket;
    struct sockaddr_in addr;
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
    
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    } 

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
    
    // Connect to server
    printf("trying to connect...\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Send file request command
    while(1){
	    printf("Enter filenames (comma-separated): \n");
	    fflush(stdout);
	    
	    char filenames[100];
	    memset(filenames,0,100);
	    fgets(filenames, sizeof(filenames), stdin);

	    send(sock, filenames, strlen(filenames), 0);
	    int bytes =0;
	   if (strncmp(filenames, "Send ", 5) == 0) {
		
			bytes = recv(sock, buffer, sizeof(buffer),0);
	   	 	if (bytes > 0) {
				buffer[bytes] = '\0';
				printf("%s", buffer);fflush(stdout);
	   		} 
	   	 	printf("received file data\n");
		
		} 
		
		char *line,*saveptr1;
	   	 // Tokenize lines first
	    	line = strtok_r(buffer, "\n", &saveptr1);
                //printf("nextoken : %s\n",saveptr1);
	  	while (line != NULL) {
			
			data *args = (data *)malloc(sizeof(data));
		    	strncpy(args->line, line, sizeof(args->line) - 1);

			    // Create a new thread to process the line
		    	pthread_t line_thread;
		    	if (pthread_create(&line_thread, NULL, process_file_ipdata, (void *)args) != 0) {
				perror("Failed to create thread for line processing");
				free(args);
		    	}
			    
		    	pthread_detach(line_thread);
        		// Move to the next line
        		line = strtok_r(NULL, "\n", &saveptr1);
    		}			
    }
    close(sock);
    return 0;
}

void *client_server_handler(void *arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Create server socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Server socket creation failed");
        pthread_exit(NULL);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(FILEPORT);
    inet_pton(AF_INET, MY_IP, &server_addr.sin_addr);
    
    int opt=1;
    	setsockopt(server_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    // Bind and listen
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        pthread_exit(NULL);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        pthread_exit(NULL);
    }

    printf("\nListening for other clients on port %d...\n", FILEPORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("\nConnection accepted from %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
               
        struct client_info *cinfo = malloc(sizeof(struct client_info));
        if (!cinfo) {
            perror("Memory allocation failed");
            close(client_sock);
            continue;
        }
        
        cinfo->socket = client_sock;
        cinfo->addr = client_addr;
               
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, handle_client, (void *)cinfo) != 0) {
            perror("Thread creation failed");
            close(client_sock);
            free(cinfo);
        }
        
        pthread_detach(client_thread);       
    }

    close(server_sock);
    pthread_exit(NULL);
}


int receive_file(char* ip,char* filenm){
   int file_sock, file_fd;
    struct sockaddr_in file_addr;
    char buffer[BUFFER_SIZE];
    memset(buffer,0,BUFFER_SIZE);
    char msg[30],fileip[INET_ADDRSTRLEN];
    strcpy(fileip,ip);
    file_addr.sin_family = AF_INET;
    file_addr.sin_port = htons(FILEPORT);
    file_addr.sin_addr.s_addr = inet_addr(fileip);
    
    file_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (file_sock < 0) {
	perror("File socket creation failed");
	return 0;
    }
    if (connect(file_sock, (struct sockaddr*)&file_addr, sizeof(file_addr)) < 0) {
	perror("File connection failed");
	close(file_sock);
	return 0;
    }
    printf("connected to : %s",ip);
    printf("sending filename..%s\n",filenm);
    send(file_sock,filenm,strlen(filenm),0);
    
    file_fd = open(filenm, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (file_fd < 0) {
	perror("File open failed");
	close(file_sock);
	return 0;
    }
    
    ssize_t bytes_received;
    while ((bytes_received = recv(file_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        //printf("recieved %zu bytes of data...\n",bytes_received);
	write(file_fd, buffer, bytes_received);
	
    }
     close(file_fd);           
    close(file_sock);
    
    return 1;
}


void *handle_client(void *arg) {
    struct client_info *cinfo = (struct client_info *)arg;
    int client_sock = cinfo->socket;
    struct sockaddr_in client_addr = cinfo->addr;

    char filename[BUFFER_SIZE] = {0};
    char file_buff[BUFFER_SIZE] = {0};
    printf("ready to receive filename...\n");
    int bytes = recv(client_sock, filename, sizeof(filename), 0);
        printf("recieved : %s\n",filename);
        if (bytes > 0) {
           // filename[bytes] = '\0';
            //---------------------------------------------------------------------------------------------------------
            
            int file_fd = open(filename, O_RDONLY);
	    if (file_fd < 0) {
		perror("File open failed");
		close(client_sock);
		free(cinfo);
		pthread_exit(NULL);
		
	    }
	    printf("opened%s\n",filename);
	    ssize_t bytes_read;
	    do {
	        bytes_read = read(file_fd, file_buff, BUFFER_SIZE);
	        printf("sending %zu bytes of data...",bytes_read);
		send(client_sock, file_buff, bytes_read, 0);
	    }while(bytes_read>0) ;
	    printf("sending %zu bytes of data...",bytes_read);
	    send(client_sock,file_buff,bytes_read,0);       
		printf("File %s sent successfully.\n", filename);
	    close(file_fd);
            
            //---------------------------------------------------------------------------------------------------------
        }
        else{
        	perror("Filename receive failed");
        }
		close(client_sock);
		free(cinfo);
		pthread_exit(NULL);
}	

void *process_file_ipdata(void * arg){
	 data *args = (data *)arg;
	 char *line = args->line;
	 
	 char *arrow, *ip_token;
         char *saveptr2;
    
         printf("\nThread processing line: %s\n", line);

    	// Find "->" and move past it
    	arrow = strstr(line, "-> ");
    	char filenm[20] = {0};
    	
    	printf("\nLine: %s\n", line);

	// Find "->" and move past it
	arrow = strstr(line, "-> ");
	//char filenm[20]={0};
	if (arrow) {
		strncpy(filenm,line,arrow-line-1);
    	    arrow += 3;
    	    ip_token = strtok_r(arrow, ", ", &saveptr2);

	    while (ip_token != NULL) {
		printf("IP: %s\n", ip_token);
		int result = receive_file(ip_token,filenm);
		if(result){
		    //printf("result:%d  for filename : %s\n",result,filenm);
		    printf("File %s received successfully.\n", filenm);
		    break;
		}
		ip_token = strtok_r(NULL, ", ", &saveptr2);
		
	    }
	}
    	
	free(args);  // Free the allocated memory
    	pthread_exit(NULL);
	 
}
