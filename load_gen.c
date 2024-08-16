#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

/* run using: ./load_gen localhost <server port> <number of concurrent users>
   <think time (in s)> <test duration (in s)> */

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int time_up;
FILE *log_file;

// user info struct
struct user_info
{
	// user id
	int id;

	// socket info
	int portno;
	char *hostname;
	float think_time;

	// user metrics
	int total_count;
	float total_rtt;
};

// error handling function
void error(char *msg)
{
	perror(msg);
	exit(0);
}

// time diff in seconds
float time_diff(struct timeval *t2, struct timeval *t1)
{
	return (t2->tv_sec - t1->tv_sec) + (t2->tv_usec - t1->tv_usec) / 1e6;
}

// user thread function
void *user_function(void *arg)
{
	/* get user info */
	struct user_info *info = (struct user_info *)arg;

	int sockfd, n;
	char buffer[256];
	struct timeval start, end;

	struct sockaddr_in server_address;
	struct hostent *server;

	while (1)
	{
		pthread_mutex_lock(&mutex);
		if (time_up)
		{
			pthread_mutex_unlock(&mutex);
			break;
		}
		pthread_mutex_unlock(&mutex);
		/* start timer */
		gettimeofday(&start, NULL);

		/* TODO: create socket */
		int network_socket = socket(AF_INET, SOCK_STREAM, 0);

		/* TODO: set server attrs */
		server = gethostbyname(info->hostname);
		if (server == NULL)
		{
			fprintf(stderr, "ERROR, No such host\n");
			exit(0);
		}
		server_address.sin_family = AF_INET;
		server_address.sin_port = htons(info->portno);
		bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);

		/* TODO: connect to server */
		int connection_status = connect(network_socket, (struct sockaddr *)&server_address, sizeof(server_address));

		// check for error in connection
		if (connection_status == -1)
		{
			printf("There was an error in connecting to the remote socket \n\n");
			exit(0);
		}

		/* TODO: send message to server */
		char client_request[] = "GET /apart1 HTTP/1.1\r\n\r\n";

		write(network_socket, client_request, sizeof(client_request));

		/* TODO: read reply from server */
		char server_response[4000];
		bzero(server_response, sizeof(server_response));

		read(network_socket, server_response, sizeof(server_response));

		/* TODO: close socket */
		close(network_socket);
		/* end timer */
		gettimeofday(&end, NULL);

		/* if time up, break */
		

		/* TODO: update user metrics */
		info->total_rtt += time_diff(&end, &start);
		info->total_count += 1;

		/* TODO: sleep for think time */
		usleep(info->think_time * 1000000);
	}

	/* exit thread */
	fprintf(log_file, "User #%d finished\n", info->id);
	fflush(log_file);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	int user_count, portno, test_duration;
	float think_time;
	char *hostname;

	if (argc != 6)
	{
		fprintf(stderr,
				"Usage: %s <hostname> <server port> <number of concurrent users> "
				"<think time (in s)> <test duration (in s)>\n",
				argv[0]);
		exit(0);
	}

	hostname = argv[1];
	portno = atoi(argv[2]);
	user_count = atoi(argv[3]);
	think_time = atof(argv[4]);
	test_duration = atoi(argv[5]);

	// printf("Hostname: %s\n", hostname);
	// printf("Port: %d\n", portno);
	// printf("User Count: %d\n", user_count);
	// printf("Think Time: %f s\n", think_time);
	// printf("Test Duration: %d s\n", test_duration);

	/* open log file */
	log_file = fopen("load_gen.log", "w");

	pthread_t threads[user_count];
	struct user_info info[user_count];
	struct timeval start, end;

	/* start timer */
	gettimeofday(&start, NULL);
	time_up = 0;
	for (int i = 0; i < user_count; ++i)
	{
		/* TODO: initialize user info */
		info[i].id = i;
		info[i].hostname = hostname;
		info[i].portno = portno;
		info[i].think_time = think_time;
		info[i].total_count = 0;
		info[i].total_rtt = 0.0;

		/* TODO: create user thread */

		pthread_create(&threads[i], NULL, &user_function, (void *)&info[i]);

		fprintf(log_file, "Created thread %d\n", i);
	}

	/* TODO: wait for test duration */
	sleep(test_duration);

	fprintf(log_file, "Woke up\n");

	/* end timer */
	pthread_mutex_lock(&mutex);
	time_up = 1;
	pthread_mutex_unlock(&mutex);
	gettimeofday(&end, NULL);

	/* TODO: wait for all threads to finish */
	// for (int i = 0; i < user_count; i++)
	// {
	// 	pthread_join(threads[i], NULL);
	// }

	/* TODO: print results */
	int total_req = 0;
	float total_rtt = 0.0;
	for (int i = 0; i < user_count; i++)
	{
		total_req += info[i].total_count;
		total_rtt += info[i].total_rtt;
	}

	float avg_throughput = (total_req*1.0) / (test_duration*1.0);
	float avg_rtt = (total_rtt*1.0) / (total_req*1.0);
	printf("average througput : %f\n", avg_throughput);
	printf("average RTT : %f\n", avg_rtt);

	/* close log file */
	fclose(log_file);

	return 0;
}