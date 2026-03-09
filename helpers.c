#include "server.h"

#define SUPP_RESP_HEADS 4
char *supported_response_headers[] = {
    "content-type", "content-length", "connection", "location" // Stored in a chosen canonical form of all lowercase.
};

/*
    Creates a socket of family and socktype and binds it at a local port. Returns the socket 
    descriptor. On fatal error, it prints the error message on stderr and returns -1
*/
int bind_to_port(int family, int socktype, const char *port)
{
    int status, server_sockfd, yes;
    struct addrinfo hints, *p, *res;
    yes = 1;
    char ip[INET6_ADDRSTRLEN];

    // Prepare the getaddrinfo call
    memset(&hints, '\0', sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = socktype;
    hints.ai_flags = AI_PASSIVE;

    // Make getaddrinfo call
    if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0)
    {
        fprintf(stderr, "%s\n", gai_strerror(status));
        return -1;
    }

    // Search linked list and bind to the first socket address you can.
    for (p = res; p != NULL; p = p->ai_next)
    {
        // socket call
        if ((server_sockfd = socket(p->ai_family, p->ai_socktype, 0)) == -1)
        {
            perror("server socket");
            continue;
        }
        // Enable immediate reuse of addresses in bind (useful on quick program restarts)
        if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("server setsockopt");
            close(server_sockfd);
            continue;
        }
        // bind socket to a local port
        if (bind(server_sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("server bind");
            close(server_sockfd);
            continue;
        }
        printf("Listening for connections on port %s...\n", port);
        if (p->ai_family == AF_INET)
        {
            inet_ntop(AF_INET, get_sin_addr(p->ai_addr), ip, INET6_ADDRSTRLEN);
            printf("Available in http://%s:%s/\n\n", ip,port);
        }
        else
        {
            inet_ntop(AF_INET6, get_sin_addr(p->ai_addr), ip, INET6_ADDRSTRLEN);
            printf("Available in http://%s:%s/\n\n", ip,port);
        }
        break;
    }
    freeaddrinfo(res); 

    if (p == NULL)
    {
        fprintf(stderr, "Failed to bind\n");
        return -1;
    }
    return server_sockfd;   
}

// Returns a pointer to the IP address structure pointed to by the socket address *p
void *get_sin_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) 
        return &((struct sockaddr_in *) sa)->sin_addr;
    else 
        return &((struct sockaddr_in6 *) sa)->sin6_addr;
}


/*
    Attempt to send len ammount of bytes from buffer buf to sockfd through the socket.
    At success return 0. At send error return -1. 
    In any case, len is set to the number of bytes actually sent.
*/
int send_all(int client_sockfd, char *buf, int *len)
{
    int bytes_total = 0, bytes_sent;
    while (bytes_total < *len)
    {
        // send data
        bytes_sent = send(client_sockfd, buf + bytes_total, *len - bytes_total, 0);
        if (bytes_sent == -1)
        {
            *len = bytes_total;
            return -1;
        }
        // update bytes_total
        bytes_total += bytes_sent;
    }
    *len = bytes_total;
    return 0;
}


/*
    Reads the HTTP request into a dynamically sized buffer and sets *len to the actual size of the request.
    On error, returns a negative integer status code and possibly logs an error message on stderr.
    Error Codes semantics:
    -1: Any server error other than recv.
    -2: Client error: Recv error or premature client connection close. 
    -3: Request exceeded allowed size limit. 
*/
int read_request(int client_sockfd, char **buf, int *len)
{
    int bytes_received, bytes_total;
    int n = 2, ralloc = 0; // Buffer Reallocation factor, startinf at 2, meaning first reallocation happens with 2 * BUFF_SIZE 
    bytes_total = 0;
    while ((bytes_received = recv(client_sockfd, *buf + bytes_total, *len - bytes_total, 0)) > 0)
    {
        bytes_total += bytes_received;
        char *lf;
        if ((lf = memmem(*buf, bytes_total, "\r\n\r\n", strlen("\r\n\r\n"))) != NULL)
        {                    
            *len = bytes_total;
            return 0;
        }
        if (bytes_total >= *len)
        {
            // Reallocate more memory.
            if  (ralloc >= MAX_REALLOC)
            {
                return -3;
            }
            char *tmp = realloc(*buf, n * BUFF_SIZE * sizeof(char));
            ralloc++;
            if (tmp == NULL)
            {
                return -1;
            }
            *buf = tmp;
            *len = n * BUFF_SIZE * sizeof(char);
            n++;
        }
    }
    if (bytes_received == -1)
    {
        perror("server recv");
        return -2;
    }
    else
    {
        fprintf(stderr, "Client closed connection before a request was read\n");
        return -2;
    }
}

// Inserts header node n onto the top of the header_node stack (linked list)
void push_node(header_node **list, header_node *n)
{
    header_node *tmp = *list; // Save the 1st list item (where list points) in a tmp var
    *list = n; 
    n->next = tmp;   
}

// Logs the headers of a request inside list into the logfile.
void log_headers(FILE *logfile, header_node *list, char *method, char *filename)
{
    fprintf(logfile, "Printing the headers of the %s request for the resource: %s...\n", method, filename);
    for (header_node *p = list; p != NULL; p = p->next)
        fprintf(logfile, "%s: %s\n", p->header_field, p->header_value);
    fprintf(logfile, "Finished printing headers\n\n");
}

// Frees the provided header node linked list of header nodes
void free_list(header_node *list)
{
    header_node *ptr = list;
    while (ptr != NULL)
    {
        header_node *next = ptr->next;
        free(ptr);
        ptr = next;
    }
}

/*
    Fills the provided list with supported, properly formatted headers to respond with.
    On success returns 0, on error returns -1. (when errors occured in neccessary headers)
    -1 corresponds to closing connection without pursuing further sends.    
*/
int fill_response_headers(int filefd, char *filename, char *uri, header_node **list, response_line *r)
{
    for (int i = 0; i < SUPP_RESP_HEADS; i++)
    {
        if (!strcmp(supported_response_headers[i], "content-type")) 
        {
            header_node *n = malloc(sizeof(header_node));
            if (n == NULL)
                return -1;
            strcpy(n->header_field, "Content-Type");
            strcpy(n->header_value, mime_type(filename));
            push_node(list, n);
        }
        else if (!strcmp(supported_response_headers[i], "connection"))
        {
            header_node *n = malloc(sizeof(header_node));
            if (n == NULL)
                return -1;
            strcpy(n->header_field, "Connection");
            strcpy(n->header_value, "close");
            push_node(list, n);
        }
        else if (!strcmp(supported_response_headers[i], "content-length"))
        {
            struct stat statbuf;
            if (fstat(filefd, &statbuf) == -1)
                return -1;
            header_node *n = malloc(sizeof(header_node));
            if (n == NULL)
                return -1;
            strcpy(n->header_field, "Content-Length");
            // Find length
            int digits = 11;
            char length_s[digits]; // Serves files up to (< 10) Giga bytes
            int size;
            long int length = (r->code != 200) ? (statbuf.st_size + strlen(r->msg)) : statbuf.st_size; //  Adds msg length since that's filled into the error template before sending it.
            if ((size = snprintf(length_s, digits,"%li", length)) < 0 || size >= digits)
                return -1; 
            strcpy(n->header_value, length_s);
            push_node(list, n);
        }
        else if (!strcmp(supported_response_headers[i], "location") && r->code == 301)
        {
            int redirection_len = strlen(uri) + 2; // +1 for null, +1 for trailling slash
            int size;
            char redirection[redirection_len];
            header_node *n = malloc(sizeof(header_node));
            if (n == NULL)
                return -1;
            strcpy(n->header_field, "Location");
            if ((size = snprintf(redirection, redirection_len, "%s/", uri)) < 0 || size >= redirection_len)
                return -1;
            strcpy(n->header_value, redirection);
            push_node(list, n);
        }
    }
    return 0;
}

// Returns the MIME/Content-Type of the given filename
char *mime_type(char *filename)
{
    int ext_len;
    char *ext, *dot, *last_dot, *search;
    search = filename;
    last_dot = NULL;
    while ((dot = strstr(search, ".")) != NULL) // Find the last dot inside filename, which indicates the file's type
    {
        last_dot = dot;
        search = last_dot + 1;
    }
    if (last_dot == NULL)
        return "application/octet-stream";
    ext= last_dot + 1; // points to the start of the extension 
    ext_len = strlen(ext);
    char lowerc_ext[ext_len + 1];
    for (int i = 0; i < ext_len + 1; i++)
        *(lowerc_ext + i) = tolower(*(ext + i));
    if (!strcmp(lowerc_ext, "html"))
        return "text/html";
    else if (!strcmp(lowerc_ext, "jpeg") || !strcmp(lowerc_ext, "jpg"))
        return "image/jpeg";
    else if (!strcmp(lowerc_ext, "png"))
        return "image/png";
    else if (!strcmp(lowerc_ext, "mp4"))
        return "video/mp4";
    else if (!strcmp(lowerc_ext, "mp3"))
        return "audio/mpeg";
    else if (!strcmp(lowerc_ext, "pdf"))
        return "application/pdf";
    else if (!strcmp(lowerc_ext, "css"))
        return "text/css";
    else
        return "application/octet-stream";
}

/*
    Sends the following through the socket: the headers and the filefd's contents,
    after insterting r->msg into the filefd's html given tag. 
    Inside the project, this function is only used with the template error.html template as filefd,
    which contains a well-known paragraph tag for this purpose. 
    Returns 0 on success, -1 on error.
    -1 corresponds to not pursuing further sends and closing connection
*/  
int serve_error_template(int client_sockfd, int filefd, char *uri,  response_line *r, char *filename, char *tag)
{
    int msg_len = strlen(r->msg), offset = 0, bytes_read;
    char *p = NULL;
    char buf[BUFF_SIZE * sizeof(char)];
    header_node *res_h_list = NULL;

    // Send response headers
    if (fill_response_headers(filefd, filename, uri, &res_h_list, r) == -1) // Fill and send response headers
        return -1;  
    // Loop into linked list and send each header
    for (header_node *p = res_h_list; p != NULL; p = p->next)
    {
        int header_len = MAX_HEADER_FIELD + MAX_HEADER_VALUE + 5; // +4 for the 4 extra chars other than field, value and +1 for '\0'
        char resp_header[header_len];
        int size; 
        if ((size = snprintf(resp_header, header_len, "%s: %s\r\n", p->header_field, p->header_value)) < 0 || size >= header_len)
            return -1;
        int rhlen = strlen(resp_header);
        if (send_all(client_sockfd, resp_header, &rhlen) == -1)
            return -1;
    }
    char *crlf = "\r\n";
    int crlf_len = strlen(crlf);
    if (send_all(client_sockfd, crlf, &crlf_len) == -1)
        return -1;

    while ((bytes_read = read(filefd, buf, BUFF_SIZE * sizeof(char))) > 0)
    {
        if ((p = memmem(buf, bytes_read, tag, strlen(tag))) != NULL)
        {
            int offset_inc = (p - buf) + strlen(tag);
            offset = offset + offset_inc; // set offset to the amount of bytes read until the end of the opening tag.
            if (send_all(client_sockfd, buf, &offset_inc) == -1)
                return -1;
            break;
        }
        else
        {
            offset += bytes_read;
            if (send_all(client_sockfd, buf, &bytes_read) == -1)
                return -1; 
        }

    }
    if (bytes_read == -1)
        return -1;
    
    if (send_all(client_sockfd, r->msg, &msg_len) == -1)
        return -1; 

    if (lseek(filefd, offset, SEEK_SET) == -1)
        return -1;
    while ((bytes_read = read(filefd, buf, BUFF_SIZE * sizeof(char))) > 0)
    {
        if (send_all(client_sockfd, buf, &bytes_read) == -1)
            return -1;
    }
    if (bytes_read == -1)
        return -1;
    free_list(res_h_list);
    return 0;
}