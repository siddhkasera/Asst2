#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
//#define NUM_CLIENT 5                                                                                                                                        
/* WORKFLOW: socket(), bind(), listen(), accept()*/
int portno;
void *connection_handler(void * threadid){

  struct sockaddr_in clientAddressInfo;
  //int threadnum = (int) threadid;                                                                                                                           
  char server_message[256] = "Reached the server\n";

  /*Creating the server socket*/
  int server_socket;
  server_socket = socket(AF_INET, SOCK_STREAM, 0);

  /*Defining the server address*/
  struct sockaddr_in server_address;
  bzero((char*)&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(portno);
  server_address.sin_addr.s_addr = INADDR_ANY;

  /*Binding the socket to our specified IP and port*/
  if(bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address))<0){
    printf("ERROR: In binding to the socket\n");
  }else{
    printf("After is\n");
  }

  listen(server_socket, 0); //server                                                                                                                          

  int client_socket;
  int clilen;
  clilen = sizeof(clientAddressInfo);
  client_socket = accept(server_socket, (struct sockaddr *) &clientAddressInfo,  &client_socket);

  if(client_socket <0){
    printf("ERROR: In accepting\n");
  }
  char buffer[256];


  int n;
  n = read(client_socket, buffer, 255);
  if(n < 0){
    printf("ERROR: In reading from the socket\n");
  }
  printf("Here is the message: %s\n", buffer);

  n = write(client_socket,"I got your message", 18);
  if( n< 0){
    printf("ERROR:In writing to the socket\n");
  }

  /*Sending the message*/
  //send(client_socket, server_message, sizeof(server_message), 0);                                                                                           

  /*Closing the server socket*/
  close(server_socket);

  return 0;
}
void * connection_handler(void * server_socket);
int main(int argc, char ** argv){

  int portnumber = atoi(argv[1]);
  printf("%d\n", portnumber);
  portno = portnumber;
  pthread_t new_thread;
  int i;
  int * pointer; // this is not                                                                                                                               

  if(pthread_create(&new_thread, NULL, connection_handler, NULL) <0){
    perror("Counld not create thread");
    return 1;

  }
  pthread_exit(NULL);
  return 0;
}

