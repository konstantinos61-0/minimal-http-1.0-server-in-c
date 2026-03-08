#define _GNU_SOURCE

#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <ctype.h>

#include <sys/socket.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <netdb.h>

#include <signal.h>
#include <stdint.h>     
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


// Macros, global variables


// socket 
#define MY_PORT "8080"
#define BACKLOG 15
#define BUFF_SIZE 256
#define MAX_REALLOC 5

// Parser (FSM) 
enum states {
    METHOD, URI, VERSION, CR, LF, HF, HVAL, CR_F, LF_F, FAILURE_400, FAILURE_403, FAILURE_404, FAILURE_500, FAILURE_501,
    FAILURE_400_METHOD, FAILURE_501_METHOD
};

// Lengths of various HTTP contructs
#define RESPONSE_LINE_LEN 256
#define FILENAME_LEN 256
#define MAX_HEADER_FIELD 256
#define MAX_HEADER_VALUE 256
#define MAX_METHOD 4
#define VERSION_LEN 8
#define MAX_URI 256


// structs

// Stores response line information
typedef struct response_line {
    int code;
    char *phrase;
    char *msg;
} response_line;

// Stores header field name - value pairs 
typedef struct header_node {
    char header_field[MAX_HEADER_FIELD + 1];
    char header_value[MAX_HEADER_VALUE + 1];
    struct header_node *next;
} header_node;


// Prototypes


void handle_connection(int client_sockfd, int root_dir);
// Parser functions
void method_trans(char current, enum states *state, int *n, char *method);
void uri_trans(char current, enum states *state, int *n, char *uri, char *filename);
void vers_trans(char current, enum states *state, int *n, char *vers);
void cr_trans(char current, enum states *state);
void lf_trans(char **current, enum states *state, int *n);
void hf_trans(char current, enum states *state, int *n, int *n_value, header_node *node);
void hval_trans(char current, enum states *state, int *n, header_node *node, header_node **list);
void cr_f_trans(char current, enum states *state);

// Helper functions associated directly with sockets
int send_all(int client_sockfd, char *buf, int *len);
int bind_to_port(int family, int socktype, const char *port);
void *get_sin_addr(struct sockaddr *p);

// Helper functions associated with request/response handling
int fill_response_headers(int filefd, char *filename, char *uri, header_node **list, response_line *r);
int read_request(int client_sockfd, char **buf, int *len);
char *mime_type(char *filename);
int serve_error_template(int client_sockfd, int filefd, char *uri,  response_line *r, char *filename, char *tag);
void print_list(header_node *list);
void push_node(header_node **list, header_node *n);
void free_list(header_node *list);