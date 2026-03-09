#include "server.h"

// ready-to-use response line structs
response_line c200 = {
    200, "OK", NULL
};

response_line c400 = {
    400, "Bad Request", "Error 400: Bad Request"
};

response_line c404 = {
    404, "Not Found", "Error 404: File Not Found"
};

response_line c403 = {
    403, "Forbidden", "Error 403: Forbidden"
};

response_line c500 = {
    500, "Internal Server Error", "Error 500: Internal Server Error"
};

response_line c501 = {
    501, "Not Implemented", "501: Not Implemented"
};

response_line c301 = {
    301, "Moved Permanently", "301: Moved Permanently"
};

/*
    This file contains only this function.
    It implements the request parser (FSM), generates the appropriate response
    and attemts to send it back to the client 
    It runs once for each connection.
    On socket send/recv error, a clean up is performed and the connection is closed.
*/
void handle_connection(int client_sockfd, int root_dir) 
{   
    // Static file variables
    int filefd;
    char filename_original[MAX_URI]; // MAX_URI -1 as "/" from URI will be omitted. 
    char filename_final[MAX_URI]; // Will be the filename of the actual file sent back (error template or original file depending on response code)
    char uri[MAX_URI + 1]; // +1 for null termination byte. Always remember it!!
    memset(uri, '\0', MAX_URI + 1);

    // buffer variables
    char res_buf[BUFF_SIZE];
    char *req_buf = malloc(BUFF_SIZE * sizeof(char)); // May get reallocated and quite big, so use heap.
    int req_len = BUFF_SIZE * sizeof(char), res_len = BUFF_SIZE * sizeof(char);
    int m;

    // parsing variables
    int n_method = 0, n_vers = 0, n_uri = 0, n_field = 0, n_value = 0;
    char method[MAX_METHOD + 1], vers[VERSION_LEN + 1];
    response_line results; // Will be modified to mirror the request parsing's outcome.
    enum states state = METHOD, old_state; // FSM state initialization
    header_node *req_h_list = NULL, *res_h_list = NULL, *n;// header linked lists and node
    memset(&results, '\0', sizeof results);

    if (req_buf == NULL)
    {
        results = c500;
        state = FAILURE_500; // Causes a 500 response before any request parsing
    }

    if ((m = read_request(client_sockfd, &req_buf, &req_len)) == -1)// recv error 
    {
        results = c500;
        state = FAILURE_500; 
    }
    else if (m == -2) // Client closed connection prematurely
    {
        free(req_buf);
        return;
    } 
    else if (m == -3) // Request too big.
    {
        results = c400;
        results.msg = "Request exceeded size limit";
        state = FAILURE_400;
    }
    // Character Input, consumed one char at a time.
    char *current = req_buf;

    while (current < (req_buf + req_len) && state < LF_F) // Request-Line Parser Loop
    {
        old_state = state; // For determining the state before and after some transitions
        switch (state)
        {
            case METHOD:
                method_trans(*current, &state, &n_method, method);
                break;
            case URI:
                uri_trans(*current, &state, &n_uri, uri, filename_original);
                break;
            case VERSION:
                vers_trans(*current, &state, &n_vers, vers);
                break;
            case CR:
                cr_trans(*current, &state);
                break;
            case LF:
                lf_trans(&current, &state, &n_field);
                if (state == HF && old_state == LF)
                {
                    n = malloc(sizeof(header_node));
                    if (n == NULL)
                        state = FAILURE_500;
                }
                continue; // The current input is incremented (or not, depending on input) inside of lf_trans 
            case HF:
                hf_trans(*current, &state, &n_field, &n_value, n); 
                break; 
            case HVAL:
                hval_trans(*current, &state, &n_value, n, &req_h_list);
                break;
            case CR_F:
                cr_f_trans(*current, &state);
                break;
        }
        current++; 
    } // End Request-Line Parser Loop
    
    free(req_buf); // Don't need request buffering after parsing it.
    
    // log the request headers inside a log directory.
    FILE *logfile;
    if ((logfile = fopen("logs/header_logs.txt", "a")) == NULL)
        logfile = stdout; 
    log_headers(logfile, req_h_list, method, filename_original); 
    if (logfile != NULL)
        fclose(logfile); 

    // Set results variable according to the request parser's outcome (e.g. c200 for 200 OK)
    if (state == LF_F)
    {
        if (strstr(uri, "..") != NULL) // Test for forbidden directory traversal
            results =  c403;
        else
            results = c200;
    }
    else if (state == FAILURE_400 || state == FAILURE_400_METHOD)
        results = c400;
    else if (state == FAILURE_500)
         results = c500;
    else if (state == FAILURE_501 || state == FAILURE_501_METHOD)
         results = c501;


    // If 200 OK, Try to open the Requested resource (static file) 
    if (results.code == 200)
    {
        struct stat sb;    
        if ((filefd = openat(root_dir, filename_original, O_RDONLY)) == -1 && errno == ENOENT)
            results = c404;
        else if (filefd == -1 && errno == EACCES)
        {
            results = c403;
            results.msg = "Error 403: Persmissions to read the requested file are not granted";
        }
        else if (filefd == -1)
            results = c500;
        // Issue a trailling slash redirection in case filefd is a directory.
        else if (fstat(filefd, &sb) != -1 && (sb.st_mode & S_IFMT) == S_IFDIR)
        {
            results = c301;
            results.msg = "301: The Requested Resource is a directory. Please follow the Location header to get to the directory's index page.";
        }
    }

    // Here, only one of the following is true: (results == c200 and filefd != -1) or (results != 200 and filefd == -1)

    // On results corresponding to some error, open the error html template and set that to filename_final.
    if (results.code != 200)
    {
        strcpy(filename_final, "templates/error.html");
        if ((filefd = openat(root_dir, filename_final, O_RDONLY)) == -1)
        {
            free_list(req_h_list);
            perror("server open");
            return;
        }
    }     
    else // On results corresponding to success, set final_filename equals to original.
        strcpy(filename_final, filename_original);



    // filefd has its final value: the file that server will attempt to send (error template or requested static file).

    // After parsing the request and opening the appropriate files, results variable mirrors the valid http response that the server will attempt to send through the socket.

    // Send the response 
    
    // Send response line
    char response_line[RESPONSE_LINE_LEN + 1];
    int bytes_read, size;
    if ((size = snprintf(response_line, RESPONSE_LINE_LEN + 1,"HTTP/1.0 %i %s\r\n", results.code, results.phrase)) < 0 || size >= RESPONSE_LINE_LEN + 1)
    {
        free_list(req_h_list);
        close(filefd);
        fprintf(stderr, "server response line format error\n");
        return;
    }
    int res_line_len = strlen(response_line);
    if (send_all(client_sockfd, response_line, &res_line_len) == -1)
    {
        perror("server send");
        close(filefd);
        free_list(req_h_list);
        return;
    }

    // Send response headers and body in case of error response (!= 200) and return. 
    if (results.code != 200)
    {
        // error.html contains a p tag with id="msg" where the results.msg will be inserted before being sent.
        if (serve_error_template(client_sockfd, filefd, uri, &results, filename_final, "<p id=\"msg\">") != -1) // Serves headers as well
        {
            // Diagnostic info 
            if (state < FAILURE_400_METHOD)
                printf("\033[31m%s %s %i %s\n\033[0m", method, filename_original, results.code, results.phrase);
            else
                printf("\033[31mMETHOD %i %s\n\033[0m", results.code, results.phrase); 
            close(filefd);
            free_list(req_h_list);
            return;
        }
        else
        {
            close(filefd);
            free_list(req_h_list);
            fprintf(stderr, "Sending error template failed\n");
            return;
        }
    }

    // Send response headers in case of success response
    if (fill_response_headers(filefd, filename_final, uri, &res_h_list, &results) == -1) // Fills res_h_list with response headers 
    {
        close(filefd);
        free_list(res_h_list);
        free_list(req_h_list);
        fprintf(stderr, "Creating response headers failed\n");
        return;
    }
    // Send all headers
    for (header_node *p = res_h_list; p != NULL; p = p->next)
    {
        char resp_header[MAX_HEADER_FIELD + MAX_HEADER_VALUE + 5]; // +4 for the 4 extra chars for field, value formatting. +1 for '\0'
        sprintf(resp_header,"%s: %s\r\n", p->header_field, p->header_value);
        int rhlen = strlen(resp_header);
        if (send_all(client_sockfd, resp_header, &rhlen) == -1)
        { 
            perror("server send");
            close(filefd);
            free_list(req_h_list);
            free_list(res_h_list);
            return;
        }
    }
    char *crlf = "\r\n";
    int crlf_len = strlen(crlf);
    if (send_all(client_sockfd, crlf, &crlf_len) == -1)
    {
        perror("server send");
        free_list(req_h_list);
        close(filefd);
        return;
    }

    // Send response body
    while ((bytes_read = read(filefd, res_buf, res_len)) > 0)
    {
        if (send_all(client_sockfd, res_buf, &bytes_read) == -1)
        {
            free_list(req_h_list);
            free_list(res_h_list);
            close(filefd);
            perror("server send");
            return;
        } 
    }
    if (bytes_read == -1)
    {
        free_list(req_h_list);
        free_list(res_h_list);
        close(filefd);
        perror("server read");
        return;
    }
    
    // Diagnostic info 
    printf("\033[32m%s %s %i %s\n\033[0m", method, filename_original, results.code, results.phrase);

    close(filefd);
    free_list(req_h_list);
    free_list(res_h_list);
    return;
}

