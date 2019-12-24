#include <stdio.h> // for perror
#include <stdint.h>
#include <string.h> // for memset
#include <stdlib.h> // for atoi
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <pthread.h>
#include <netdb.h>
#include <iostream>
#include <list>

using namespace std;

typedef struct {
	int childid;
	int socketfd;
	char *buf;
} MultipleArg;

pthread_mutex_t mutex;

void dump(uint8_t* str, int num){
	for(int i = 0; i < num ; ++i){
		printf("%x ", str[i]);
	}
}

void *client_listen(void *arg){
    MultipleArg *fd = (MultipleArg *)arg;
	
	ssize_t sent = send(fd->socketfd, fd->buf, strlen(fd->buf), 0);

	if(sent == 0){
		perror("send error");
		exit(0);
	}
}

void *server_listen(void *arg){
    MultipleArg *fd = (MultipleArg *)arg;

	const static int BUFSIZE = 1024;
	char buf[BUFSIZE];

	ssize_t received = recv(fd->socketfd, buf, BUFSIZE - 1, 0);

	if (received == 0 || received == -1) {
		perror("recv failed");
		exit(0);
	}

	buf[received] = '\0';
	printf("%s\n", buf);

	ssize_t sent = send(fd->childid, buf, strlen(buf), 0);

	if(sent == 0){
		perror("send error2");
		exit(0);
	}
	free(arg);
}

char * find_host(char * buf, int received){
	int indexOfHost = 0;
	char * host;

	while(buf[indexOfHost] != 0xd || buf[indexOfHost + 1] != 0xa || memcmp(buf + indexOfHost + 2, "Host", 4) != 0){
			indexOfHost++;
			if(indexOfHost == received){
				printf("No host\n");
				break;
			}
		}

	if(indexOfHost == received){
		return NULL;
	}

	printf("%d index\n", indexOfHost);
	
	int index = indexOfHost + 1;
	while(buf[index] != 0xd){
		index++;
	}
	buf[index] = 0;

	host = (char *)malloc(index - indexOfHost - 7);

	printf("%d size", index - indexOfHost - 6);

	dump((uint8_t *)buf + indexOfHost + 8, index - indexOfHost -8);
	memcpy(host, buf + indexOfHost + 8, index - indexOfHost - 7);
	buf[index] = 0xd;
	
	
	return host;
}

int main(int argc, char* argv[]) {

	if(argc != 3){
		printf("wrong argumnets");
		return -1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	pthread_mutex_init(&mutex, NULL);

	while (true) {
		pthread_t p_thread1;
        pthread_t p_thread2;
		struct sockaddr_in addr;
		int thr_id1 = 0;
        int thr_id2 = 0;
		socklen_t clientlen = sizeof(sockaddr);
		struct hostent* host_addr;
        
        int childfd;
		char * host;

		childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}
		printf("connected\n");

		const static int BUFSIZE = 1024;
    	char buf[BUFSIZE];

		ssize_t received = recv(childfd, buf, BUFSIZE - 1, 0);
		if (received == 0 || received == -1) {
			perror("recv failed");
			exit(0);
		}
		buf[received] = '\0';
		printf("%s\n", buf);

		host = find_host(buf, received);

		printf("Host : %s", host);

		host_addr = gethostbyname(host);

		if(host_addr->h_addr_list == NULL){
			perror("not allowed host");
			exit(0);
		}

		dump((uint8_t*)(host_addr->h_addr_list[0]), 4);

		int server_socket = socket(AF_INET, SOCK_STREAM, 0);

		if(server_socket == -1){
			perror("socket create error");
			exit(0);
		}

		struct sockaddr_in addr2;
		addr2.sin_family = AF_INET;
		addr2.sin_port = htons(80);
		addr2.sin_addr.s_addr = *((int *)(host_addr->h_addr_list[0]));
		memset(addr2.sin_zero, 0, sizeof(addr2.sin_zero));

		int res = connect(server_socket, reinterpret_cast<struct sockaddr*>(&addr2), sizeof(struct sockaddr));

		if(res == -1){
			perror("connection failed");
			exit(0);
		}

		MultipleArg * arg = (MultipleArg *)malloc(sizeof(MultipleArg));
		arg->childid = childfd;
		arg->socketfd = server_socket;
		arg->buf = buf;

		thr_id1 = pthread_create(&p_thread1, NULL, client_listen, (void *)arg);

		if(thr_id1 < 0){
			perror("thread create error");
			exit(0);
		}

        thr_id2 = pthread_create(&p_thread2, NULL, server_listen, (void *)arg);

        if(thr_id2 < 0){
            perror("thread create error");
            exit(0);
        }

		free(host);
	}

	close(sockfd);

	return 1;
}