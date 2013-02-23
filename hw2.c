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
#include <pthread.h>

#define BUFFER_SIZE 2048
#define PAGE_NOT_FOUND "<!doctype html><html><head><title>404 Not Found</title></head><body><h1>You Must Be New Here</h1><p>Whatever you were looking for isn\'t here. You need a <a href=\"http://internet-map.net/#10-115.88082184213471-69.08807438100536\">map</a>.</p></body></html>"
#define BAD_REQUEST "<!doctype html><html><head><title>400 Bad Request</title></head><body><h1>Such Language!</h1><p>Never have I heard such HTTP verbs! This server only supports GET requests.</p></body></html>"
#define MAX_THREADS 1000

static char Docroot[128];
static pthread_t Threads[MAX_THREADS];
extern int errno;

void put( int file, char *msg ) {
    int written = 0;
    int msg_bytes = strlen(msg)*sizeof(char);
    while ( written < msg_bytes ) {
	written += write(file, msg+(written/sizeof(char)), msg_bytes - written);
    }
}

int is_dir( char *path ) {
    struct stat s;
    if ( stat( path, &s ) < 0 ) {
	if ( ENOENT == errno ) {
	    return 0;
	}
	perror("stat");
    }
    return S_ISDIR(s.st_mode);
}

void *handle_request( void *arg ) {
    int sock = (int)(intptr_t)arg;
    
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
    int fd;
    int response_code = 200;
    char response_message[64];
    strcpy(response_message, "OK");
    // Check request method
    if ( 0 != strcasecmp(http_method, "GET") ) {
	response_code = 400;
	strcpy(response_message, "Bad Request");
    }
    // Good requests only
    else {
	sprintf(file_path, "%s%s", Docroot, request_path);
	// Special handling for directories
	if ( is_dir(file_path) ) {
	    sprintf(file_path, "%s/index.html", Docroot);
	}
	// Open the document to be served
	if ( (fd = open( file_path, O_RDONLY )) < 0 ) {
	    // File not found
	    if ( ENOENT == errno ) {
		response_code = 404;
		strcpy(response_message, "Not Found");
	    } else {
		perror("open");
		exit(1);
	    }
	}
    }
    char content_type[64];
    strcpy(content_type, "text/html");
    if ( 200 == response_code ) {
	// Find the content-type of the document
	char *filename = strrchr( file_path, '.' );
	if ( !strcasecmp(filename, ".png") ){
	    strcpy(content_type, "image/png");
	} else if ( !strcasecmp(filename, ".gif") ) {
	    strcpy(content_type, "image/gif");
	} else if ( !(strcasecmp(filename, ".jpg")&strcasecmp(filename, ".jpeg")) ) {
	    strcpy(content_type, "image/jpeg");
	} else if ( !strcasecmp(filename, ".pdf") ) {
	    strcpy(content_type, "application/pdf");
	}
    }
    // Send a response
    char response_header[1024];
    sprintf(response_header, "HTTP/1.0 %d %s\r\nContent-type: %s\r\n\r\n",
	    response_code, response_message, content_type);
    put(sock, response_header);
    switch ( response_code ) {
    case 200:
    {
	// Read in the file
	char file_buff[BUFFER_SIZE];
	int bytes_read;
	while ( (bytes_read = read(fd, file_buff, BUFFER_SIZE)) ) {
	    int written = write(sock, file_buff, bytes_read);
	}
	close(fd);
    } break;
    case 400:
    {
	put(sock, BAD_REQUEST);
    } break;
    case 404:
    {
	put(sock, PAGE_NOT_FOUND);
    } break;
    }

    shutdown(sock,SHUT_RDWR);
    close(sock);

    return NULL;
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    if ( argc < 3 ) {
	fputs("USAGE: ./hw2 <port> <directory>\n", stderr);
	exit(1);
    }
    int port_num = atoi(argv[1]);
    strncpy(Docroot, argv[2], 128);

    // Make sure that docroot specifies a directory
    if (! is_dir(Docroot) ) {
	fprintf(stderr, "%s does not specify a directory.\n", Docroot);
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
    int cur_thread;

    while(1) {
	// wait for a connection
	res = listen(server_sock,0);
	if(res < 0) {
	    perror("Error listening for connection");
	    exit(1);
	}

	int sock;
	sock = accept(server_sock, (struct sockaddr*)&remote_addr, &socklen);
	puts("accepted a connection");
	if(sock < 0) {
	    perror("Error accepting connection");
	    exit(1);
	}
	if ( pthread_create(&Threads[cur_thread], NULL, handle_request,
			    (void*)(intptr_t)sock) ) {
	    fputs("error creating thread", stderr);
	    exit(1);
	}
	cur_thread = (cur_thread+1)%MAX_THREADS;
    }

    shutdown(server_sock,SHUT_RDWR);
}
