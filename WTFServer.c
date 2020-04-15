#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if(argc != 2) {
        printf("ERROR: Incorrect Number of Arguments");
    }
    char server_message[256] = "You have reached the server";

    // Create Server Socket
    int server_socket;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    // Define the Server Address
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[1]));
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to specified IP and port
    bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address));

    // Listen to connections
    listen(server_socket, 5);

    // Accept Connections 
    int client_socket;
    client_socket = accept(server_socket, NULL, NULL);

    // Send data to client socket
    send(client_socket, server_message, sizeof(server_message), 0);

    // Close Server Socket
    close(server_socket);
       
    return 0;
}