#include "server.h"
/*
    Usage: server root_dir [port]
*/
void sigchld_handler(int signo); 

int main(int argc, char *argv[])
{
    struct sockaddr_storage their_addr;
    socklen_t addrlen;
    int server_sockfd, client_sockfd, root_dir;
    char *port;
    char ip[INET6_ADDRSTRLEN];

    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s root_dir [port]\n", argv[0]);
        return 1;
    }

    // Port configuration
    if (argc == 2)
        port = MY_PORT;
    else
        port = argv[2];

    // Root directory configuration
    if ((root_dir = open(argv[1], O_RDONLY)) == -1)
    {
        perror("server root dir open");
        return 3;
    }

    if ((server_sockfd = bind_to_port(AF_UNSPEC, SOCK_STREAM, port)) == -1) // socket & bind call
        return 4; // perror is called inside bind_to_port

    if (listen(server_sockfd, BACKLOG) == -1) 
    {
        perror("server listen");
        return 5;
    }

    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART; // If signal interupts sys calls, restart them (accept sys call protection inside accept loop)
    act.sa_handler = sigchld_handler; 

    if (sigaction(SIGCHLD, &act, NULL) == -1) // set act as the sigaction of SIGCHLD
    {
        perror("server sigaction");
        return 6;
    }
    while (1) 
    { // Accept Loop, socketfd mode: block. (default)
        addrlen = sizeof(struct sockaddr_storage);
        if ((client_sockfd = accept(server_sockfd, (struct sockaddr *) &their_addr, &addrlen)) == -1) // blocking call
        {
            perror("server accept");
            continue; // if accept fails, continue to next queued connection.
        }
        if (their_addr.ss_family == AF_INET)
        {
            inet_ntop(AF_INET, get_sin_addr((struct sockaddr *)&their_addr), ip, INET6_ADDRSTRLEN);
            printf("Connection Established with client at address: %s\n", ip);
        }
        else
        {
            inet_ntop(AF_INET6, get_sin_addr((struct sockaddr *)&their_addr), ip, INET6_ADDRSTRLEN);
            printf("Connection Established with client at address: %s\n", ip);
        }
        int child_id;
        if ((child_id = fork()) == 0)
        { // Inside child process
            close(server_sockfd); // child doesn't need the listener 
            handle_connection(client_sockfd, root_dir);
            close(client_sockfd);
            printf("Closed connection\n\n" );
            exit(0);
        }
        else if (child_id == -1)
        {
            perror("server fork");
        }
        close(client_sockfd); // parent doesn't need the client socket
    }
    // cleanup
    close(server_sockfd);
    close(root_dir);
    return 0;
}


// Handler for the SIGCHLD signal (child termination) so that the server cleans up any zombie processes.
void sigchld_handler(int signo)
{
    (void)signo; // quiet the warning about unused signo variable.

    int restore_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0); // Non-blocking call, wait any terminated child process.
    errno = restore_errno; // Restore global errno in case waitpid changed it.
}
