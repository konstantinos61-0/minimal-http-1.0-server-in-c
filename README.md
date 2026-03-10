# Minimal HTTP/1.0 server in C
## Overview
This project is a CLI program written in C. It is an http server program for serving static files from the host machine. More specifically, it's a partial implementation of HTTP/1.0 over TCP/IP connections, utilizing the Linux socket interface. Although minimal, it was designed with extensibility in mind (see FSM states section). The choice of the older version 1.0 was deliberate. I believe it balances my project's scope between it being a valuable learning experience for me while also remaining relevant to real world work. A makefile is included, so running `make` builds the executable file named "server".  
  
**Video Demo**: https://www.youtube.com/watch?v=U7P2p216XVk

**Program Usage**: path/to/server root_dir [port]  
- **server**: the executable filename produced by make
- **root_dir**: The directory that the server will use as its root for serving files.
- **port**: The port number that the server will bind to on the host machine. If omitted, 8080 is used by default.

## Implemented HTTP/1.0 Features

The following features from the RFC 1945 specification are implemented:
- **methods**: GET
- **response codes**: 200, 301, 400, 403, 404, 500, 501
- **request headers**: Accepts and stores any properly formatted request headers.
- **response headers**: Always respond with the headers: Content-Length, Content-Type, Connection (value=close always, as persistent connections aren't supported). Include the Location header when necessary.  
- **URIs**: absolute path form 

Server accepts HTTP/1.1 version tokens but always responds with version 1.0.

## Source File Organization

The program is organized into multiple source files so that each one has a clear responsibility:
- **server.c** contains the main function with the main accept server loop and a signal handler for child process termination.
- **connection_handler.c** handles the HTTP request reading, parsing and response generation 
- **helpers.c** contains all the utility functions, written to abstract details and improve readability
- **transitions.c** contains all the transition functions of the request parser (Finite State Machine) and some test functions used inside them.
- **server.h**: Since this is not a large-sized project, it felt natural to centralize most of the shared definitions/declarations into this single header file. This achieves easier maintenance. 

## Source file description

### server.c
Contains the main function. Inside of main, correct usage is firstly checked, followed by port and root directory configuration. Then, the program opens a TCP/IP socket, binds it on the local machine's specified port and starts listening on that socket for incoming connection requests. Then, the infinite "accept" loop is entered. Each iteration performs a blocking accept call to acquire the next queued up connection request and forks a child process to handle that connection. Each connection handles a single request per the HTTP/1.0 specification. A signal handler is also defined and placed upon the SIGCHLD kernel signal to reap any child zombie processes.

### connection_handler.c
Contains the handle_connection function. Inside it, the incoming request is read into a dynamically sized buffer and then run through the request parser character by character. Depending on its outcome, the program will either attempt to open the requested resource (static file) or error template. Path traversals through ".." are forbidden for server security. URLs pointing to directories are redirected to the directory's index (trailing slash redirection). Then, the program proceeds to send the resource's contents through the socket, wrapping it inside a proper HTTP response line and headers. Consistent error checking is performed so that the program at least terminates with an appropriate error log at a fatal error. 

### Request Parser 
The request parser implementation is based on a Finite State Machine (FSM) abstract model. In particular, its designed as a Deterministic FSM of the transducer style which means that as it consumes input and transitions between states it also produces output. This model is explained in detail below.

#### FSM parser Specification, Design
The parser accepts or rejects a given request based on the validity of its components and whether the requested feature is actually implemented.
This happens by transitioning to the appropriate final failure or success states associated with a certain response code.
The parser:
1. Rejects methods HEAD and POST with a failure code of 501.
2. Rejects any other non-GET method tokens with a failure code of 400. 
3. Rejects absolute paths not starting with "/" with a failure code of 400.
4. Rejects HTTP version tokens other than HTTP/1.0 and HTTP/1.1 with a failure code of 400.
5. Rejects requests containing invalid header field names with a code of 400.
6. Issues a 500 response in case of an internal server error.
7. Accepts all other properly formatted GET requests. 
8. Stores all valid field name - value pairs  

Inputs:
- The input alphabet is the set of all bytes [0, 255].

Outputs:
- The output alphabet is the same as the input. The output bytes are stored inside string variables that incrementally build the final parsed tokens.

States:

Each state indicates the http token that is currently being parsed when we're at that state.
- METHOD: Method is being parsed. URI: uri is being parsed, etc.
- Exceptions:
    - CR, CR_F: byte '\r' has just been parsed
    - LF, SUCCESS_GET: byte '\n' has just been parsed. SUCCESS_GET also indicates that the final LF byte has been read and parsing was successful with a corresponding GET method 
    - FAILURE_XXX: (where XXX is any possible response code implemented other than 200) Request rejected with a failure code XXX

This part of the design specifically promotes feature extensibility. For example, if we wanted to implement the method HEAD, we would add a new succcess state, say 'SUCCESS_HEAD', and handle it with some modifications in the transition functions (see below). 

### transitions.c
This file implements the behaviour of the FSM request parser via the transition functions and some test functions. Each state has a corresponding transition function which encodes the logic of the state transitions for each possible state, for each possible input. The resulting bundle of functions makes up the FSM's behaviour which must adhere to the above specification. 

### helpers.c
Contains utility functions used throughout all other source files, including:
- socket setup and binding
- request reading
- response header construction for any response
- linked list utilities for header storage and logging
- MIME type detection
- response sending, including error template serving.

### General 
Error logs are produced to the stderr stream. Some diagnostic information is also regularly sent to stdout throughout the program. Valgrind was used to make the program runs memcheck clean.

## Testing
### Testing directory
The directory `test_dir` contains several folders with html files, css, images etc. for testing purposes. The manual tests used during development are documented inside the `tests.md` file. 
### Headers
The request headers of each request are logged inside the logs/header_logs.txt file. This way it can be verified that headers are stored as documented.

