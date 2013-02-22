#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/stat.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 2048

int main(int argc, char** argv) {

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0) {
	perror("Creating socket failed: ");
	exit(1);
    }

    // allow fast reuse of ports
    int reuse_true = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true, sizeof(reuse_true));

    struct sockaddr_in addr; 	// internet socket address data structure
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT); // byte order is significant
    addr.sin_addr.s_addr = INADDR_ANY; // listen to all interfaces

    int res = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
    if(res < 0) {
	perror("Error binding to port");
	exit(1);
    }

    struct sockaddr_in remote_addr;
    unsigned int socklen = sizeof(remote_addr);

    while(1) {
	// wait for a connection
	res = listen(server_sock,0);
	if(res < 0) {
	    perror("Error listening for connection");
	    exit(1);
	}

	int sock;
	sock = accept(server_sock, (struct sockaddr*)&remote_addr, &socklen);
	if(sock < 0) {
	    perror("Error accepting connection");
	    exit(1);
	}
	puts("Accepted a connection");

	// Receive the incoming request
	char buf[BUFFER_SIZE];
	memset(&buf,0,sizeof(buf));
	int space_left = BUFFER_SIZE;
	while ( space_left > 0 && (NULL == strstr(buf, "\r\n\r\n")) ) {
	    int recv_count = recv(sock, (buf+(BUFFER_SIZE-space_left)), space_left, 0);
	    if(recv_count<0) {
		perror("Receive failed");
		exit(1);
	    }
	    space_left -= recv_count;
	}
	puts(buf);

	shutdown(sock,SHUT_RDWR);
	close(sock);
    }

    shutdown(server_sock,SHUT_RDWR);
}


