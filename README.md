# HTTP/1.0 server program (partial implementation)

### Overview

This project is a CLI program written in C. It is an http server program for serving static files from the host machine. More specifically, its a partial implementation of HTTP/1.0 over TCP connections in the Internet Domain (IP Addresses) utilizing the Linux socket interface. The choice of  the older version 1.0 was deliberate. I believe it balances my project's scope between it being a valuable learning experience for me while also remaining relevant to real world work. Running "make" builds the executable file named "server".

**Program Usage**: path/to/server root_dir [port]  
- **server**: the executable file name produced by make
- **root_dir**: The directory that the server will use as its root for serving files. For example, the URL "/my_page.html" will correspond to the file stored on the host machine at "root_dir/my_page.html"
- **port**: The port number that the server will bind to on the host machine. If omitted, 8080 is used by default.

### Implemented HTTP/1.0 Features

The following features from the RFC 1945 specification are implemented:
- **methods**: GET
- **response codes**: 200, 301, 400, 403, 404, 500, 501
- **request headers**: Accepts and stores any validly formatted request headers .
- **response headers**: Always respond with the headers: Content-Length, Content-Type, Connection (always with a value of close as persistent connections aren't supported). Include the Location header when necessary.  
- **URIs**: in absolute path form

### Source File Organization

The program is organized into multiple source files so that each one has a clear responsibility:
- **server.c** contains the main function with the main server loop for accepting incoming connections and a signal handler for child process termination.
- **connection_handler.c** handles the HTTP request reading, parsing and response generation 
- **helpers.c** contains all the smaller utility functions that are written to abstract details and improve readability
- **transitions.c** contains all the transition functions of the request parser (Finite State Machine) and some test functions used inside them.
- **server.h**: Since this is not a large-sized project, it felt natural to centralize most of the shared definitions/declarations into this single header file. This was done to achieve easier maintenance at the expense of each file not having access only to the information it needs.  

## Source file description

### server.c
Contains the main function. Inside of main, correct usage is firstly checked, followed by port and root directory configuration. Then, the program opens a TCP/IP socket, binds it on the local machine's specified port and starts listening on that socket for incoming connection requests. Then, an infinite "accept" loop is entered: Make a blocking accept call to acquire the next queued up connection request and fork a child process to handle that connection by calling handle_connection. Each connection handles a single request per the http/1.0 specification.  A signal handler is also  defined and placed upon the SIGCHLD kernel signal (child process termination) so than any terminated child processes are fetched, thus avoiding clogging the system with "zombies".  

### connection_handler.c
Contains the handle_connection function. Inside it, the incoming request is read into a dynamically sized buffer and then run through the request parser character by character in a loop. Depending on its outcome, the program will either attempt to open the requested resource (static file) or error template. Path traversals through ".." are forbidden for server security. URLs pointing to directories are redirected to the directory's index (trailing slash redirection). Then, the program proceeds to send its contents through the socket, wrapping it inside a proper HTTP response line and headers. Consistent error checking is performed so that there isn't any undefined behaviour and the program at least terminates with an appropriate error log at a fatal error. 

### Request Parser 
The request parser implementation is based on a Finite State Machine (FSM) abstract model. In particular, its designed as a Deterministic FSM of the transducer style which means that as it consumes input and transitions between states it also produces output. This model is explained in detail below.

#### FSM parser Specification, Design
The parser accepts or rejects a given request based on the validity of its components and whether the requested feature is actually implemented.
This happens by transitioning to the appropriate final failure or success states that are also associated with a certain response code with which the server will eventually respond.
The parser:
1. Rejects methods HEAD and POST with a failure code of 501.
2. Rejects any other non-GET method tokens with a failure code of 400. 
3. Rejects absolute paths not starting with "/" with a failure code of 400.
4. Rejects http version tokens other than HTTP/1.0 and HTTP/1.1 with a failure code of 400.
5. Rejects requests containing invalid header field names with a code of 400.
6. Issues a 500 response in case of an internal server error.
7. Accepts all other properly formatted GET requests. 
8. Stores all valid field name - value pairs  

Inputs:
- The input alphabet is the set of all bytes [0, 255].

Outputs:
- The output alphabet is the same as the input. The output bytes are saved as part of a particular http construct string variable.

States:

Each state indicates the http token that is currently being parsed when we're at that state.
- METHOD: Method is being parsed. URI: uri is being parsed, etc.
- Exceptions:
    - CR, CR_F: byte '\r' has just been parsed
    - LF, LF_F: byte '\n' has just been parsed. LF_F indicates that the final lf byte has been read and corresponds to a success code 200.
    - FAILURE_XXX: (where XXX is any possible response code implemented other than 200) Request rejected with a failure code XXX

### transitions.c
This file implements the behaviour of the FSM request parser via the transition functions. Each state has a corresponding transition function which takes as input a char variable (conceptually the current input), a pointer to the state and some other necessary information. These functions encode the logic of the state transitions for each possible state, for each possible input. The result is a bundle of functions that make up the FSM's behaviour which must adhere to the above specification. Also, in this file, some test functions are defined; they are called inside the transition functions to help with the decisions. 

### helpers.c
Containts utility functions used throughout all other source files. Specifically, the following functions are included:
1. bind_to_port: Creates a socket, performs DNS and port lookup and binds the socket to a local port.
2. get_sin_addr: Returns a pointer to the IP address of a given socket address
3. send_all: Attempts to send all of the data in a buffer through the socket.
4. read_request: Reads the incoming client request into a dynamically sized buffer.
5. push_node: Inserts a given header node into the provided header node linked list (stack)
6. print_list: Prints the provided header node linked list to the stdout.
7. free_list: Frees the provided linked list of header nodes
8. fill_response_headers: Fills the provided list with supported, properly formatted headers to respond with.
9. mime-type: Returns the MIME/Content-Type of the given filename. Supports html, jpg, png, mp3, mp4, pdf css types. Easily extensible.
10. serve_error_template: Sends the error template answer through the socket. This includes the response line, the headers and the file contents (filled with results message)


### General 
In case of any  function/sys call error or failure to meet the requirements checks, logs are produced to the stderr stream. Some diagnostic information is also output to stdout throughout the program. Clean up is performed regularly on any open file descriptors and heap allocated memory. Valgrind was used to make the program runs memcheck clean. 

### Testing

#### Headers
The print_list function is only used for testing purposes. Specifically, it is placed right after the request parsing and it prints all of the headers of the request. This way one can ensure that headers are handled as promised by the specification.

#### Permissions
Modify permissions with command "chmod" to any file inside test_dir in order to test the 403 response.
