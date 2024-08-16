#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>

#define THREAD_POOL_SIZE 5
#define MAX_QUEUE_SIZE 30000
#define BUFFER_SIZE 4096

int queue[MAX_QUEUE_SIZE];

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;
pthread_cond_t condition_var2 = PTHREAD_COND_INITIALIZER;
int cur_queue_size = 0;

struct http_request{
    char version[100];
    char method[100];
    char url[100];
};

struct http_request getRequestData(char server_message[256]){
    int i = 0;
    struct http_request temp;
    char *arr[2];

    char* token = strtok(server_message, "\n");
    strcpy(temp.method,strtok(token, " "));
    strcpy(temp.url,strtok(NULL, " "));
    strcpy(temp.version, "HTTP/1.1");

    return temp;
}

void handle_connection(int pclient)
{
	int client_socket = pclient;

	char server_message[256];

	char response_data[9000];
	bzero(response_data, sizeof(response_data));
	char http_header[5000];
	bzero(http_header, sizeof(http_header));

	bzero(server_message, sizeof(server_message));

	read(client_socket, (void *)server_message, sizeof(server_message));
    struct http_request request= getRequestData(server_message);
    char path[4096];
	strcpy(path, "./html_files");
    strncat(path, request.url, strlen(request.url));
	struct stat sb;
	
	if (stat(path, &sb) == 0)
	{
		if (S_ISDIR(sb.st_mode))
		{
			strcat(path, "/index.html");
		}

		FILE *html_data;
		html_data = fopen(path, "r");

		char temp[10000];
		bzero(temp, sizeof(temp));
        
		
        stat(path, &sb);
        fread(temp, sizeof(char), sb.st_size, html_data);
        temp[(sizeof temp)-1] = 0;
        // printf("%s", temp);
        fclose(html_data);

        strcat(response_data, temp);
		bzero(http_header, sizeof(http_header));
		strcat(http_header, "HTTP/1.1 200 OK\r\n");
		strcat(http_header, "Content-Type: ");
		strcat(http_header, "text/html\r\n");
		strcat(http_header, "Content-Length: ");
		char size[1000];
		sprintf(size, "%d", (int)(strlen(response_data)));
		strcat(http_header, size);
		strcat(http_header, "\r\n\n");
		strcat(http_header, response_data);

		send(client_socket, http_header, sizeof(http_header), 0);
	}
	else
	{
		char size[1000];
		strcat(http_header, "HTTP/1.1 404 NOT FOUND\r\n");
		strcpy(response_data, "<html><body> 404.... PAGE NOT FOUND </body></html>");
		strcat(http_header, "Content-Type: ");
		strcat(http_header, "text/html\r\n");
		strcat(http_header, "Content-Length: ");
		sprintf(size, "%d", (int)(strlen(response_data)));
		strcat(http_header, size);
		strcat(http_header, "\r\n\n");
		strcat(http_header, response_data);
		send(client_socket, http_header, sizeof(http_header), 0);
	}
	printf("Processed\n");
    
	close(client_socket);
}

void *thread_function(void *arg)
{

	while (1)
	{
		int pclient = -1;
		pthread_mutex_lock(&mutex);
		if (cur_queue_size == 0)
		{
			pthread_cond_wait(&condition_var, &mutex);
		}
        for(int i = 0; i < MAX_QUEUE_SIZE; i++){
            if(queue[i] != -1){
                pclient = queue[i];
                queue[i] = -1;
                cur_queue_size--;
            }
        }
		pthread_cond_signal(&condition_var2);
		pthread_mutex_unlock(&mutex);
		if(pclient != -1)
		{
			printf("Processing request\n");
			handle_connection(pclient);
            // free(pclient);

		}
	}
}

void signal_handler(int sig){
	for(int i = 0; i < THREAD_POOL_SIZE; i++){
		pthread_kill(thread_pool[i], SIGKILL);
        
	}
    for(int i = 0; i < THREAD_POOL_SIZE; i++){
		
        pthread_join(thread_pool[i], NULL);
	}
}

int main(int argc, char *argv[])
{
	signal(SIGINT, signal_handler);

	for (int i = 0; i < THREAD_POOL_SIZE; i++)
	{
		pthread_create(&thread_pool[i], NULL, thread_function, NULL);
	}
    for(int i = 0; i < MAX_QUEUE_SIZE; i++){
        queue[i] = -1;
    }
	int server_socket;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		printf("socket creation failed...\n");
		exit(1);
	}
	int portno = atoi(argv[1]);
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(portno);
	server_address.sin_addr.s_addr = INADDR_ANY;

	int x = bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
	if (x < 0)
	{
		printf("Bind Failed...\n");
		exit(1);
	}
	int y = listen(server_socket, 1000);

	if (y < 0)
	{
		printf("Listen Failed...\n");
		exit(1);
	}
	else
	{
		printf("Listening at port no. %d ...\n", portno);
	}

	int client_socket;

	while (1)
	{
		if(cur_queue_size == MAX_QUEUE_SIZE){
			pthread_cond_wait(&condition_var2, &mutex);
		}
		client_socket = accept(server_socket, NULL, NULL);
		if (client_socket < 0)
		{
			printf("Accept failed");
			continue;
		}

		pthread_mutex_lock(&mutex);
		
        for(int i = 0; i < MAX_QUEUE_SIZE; i++){
            if(queue[i] == -1){
                cur_queue_size++;
                queue[i] = client_socket;
                break;
            }
        }
		pthread_cond_signal(&condition_var);
		pthread_mutex_unlock(&mutex);
	}
	close(server_socket);
	return 0;
}