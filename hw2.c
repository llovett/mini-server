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
#include <fcntl.h>

#define BUFFER_SIZE 2048

int is_dir( char *path ) {
    struct stat s;
    stat( path, &s );
    return S_ISDIR(s.st_mode);
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    if ( argc < 3 ) {
	fputs("USAGE: ./hw2 <port> <directory>\n", stderr);
	exit(1);
    }
    int port_num = atoi(argv[1]);
    char *docroot = argv[2];
    // Make sure that docroot specifies a directory
    if (! is_dir(docroot) ) {
	fprintf(stderr, "%s does not specify a directory.\n", docroot);
	exit(1);
    }

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
    addr.sin_port = htons(port_num); // byte order is significant
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
	// Parse the request
	char http_method[16];
	char request_path[1024];
	char http_version[64];
	sscanf(buf, "%s %s %s\n", http_method, request_path, http_version);
	puts("request received ----------");
	printf("method: %s\npath: %s\nhttp version: %s\n", http_method, request_path, http_version);

	// Grab the file to serve
	char file_path[1024];
	sprintf(file_path, "%s%s", docroot, request_path);
	// Special handling for directories
	if ( is_dir(file_path) ) {
	    sprintf(file_path, "%s/index.html", docroot);
	}
	int fd;
	if ( (fd = open( file_path, O_RDONLY )) < 0 ) {
	    perror("open");
	    exit(1);
	}

	// Read in the file
	char file_buff[BUFFER_SIZE];
	int bytes_read;
	while ( (bytes_read = read(fd, file_buff, BUFFER_SIZE)) ) {
	    write(sock, file_buff, bytes_read);
	}
	close(fd);

	shutdown(sock,SHUT_RDWR);
	close(sock);
    }

    shutdown(server_sock,SHUT_RDWR);
}


