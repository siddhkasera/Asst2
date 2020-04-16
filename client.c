#include<unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include<netdb.h>
int main(int argc, char ** argv){



  int portnumber = atoi(argv[2]);
  printf("%d\n", portnumber);
 
  struct sockaddr_in serverAddressInfo;
  struct hostent *serverIPAddress;
  
  serverIPAddress = gethostbyname(argv[1]);
  if(serverIPAddress == NULL){
    printf("ERROR:So such host\n");
  }


  int network_socket; //intializing socket file descriptor.                                                   
  network_socket  = socket(AF_INET, SOCK_STREAM, 0); // calling the soket function                        
  if(network_socket < 0){
    printf("ERROR:In creating the socket\n");
  }
  bzero((char*) &serverAddressInfo, sizeof(serverAddressInfo));

  /*specify address for the socket*/
  serverAddressInfo.sin_family = AF_INET;
  serverAddressInfo.sin_port = htons(portnumber);
   
  bcopy((char*)serverIPAddress->h_addr, (char*)&serverAddressInfo.sin_addr.s_addr, serverIPAddress->h_length);
  /*Making the connection*/  

  int connection_status = connect(network_socket, (struct sockaddr *) &serverAddressInfo, sizeof(serverAddressInfo));
  /*checking the connection */

  if(connection_status == -1){
  printf("ERROR: In establishing a connection with the server.\n");
   }
  char buffer[256];  
  printf("Please enter the message: ");
  bzero(buffer,256);
  
  fgets(buffer, 255, stdin);
  int n;
  n = write(network_socket, buffer, strlen(buffer));
  if(n<0){
    printf("ERROR:In writing to the socket\n");
  }
  bzero(buffer, 256);
  n = read(network_socket, buffer, 255);
  if(n <0){
    printf("ERROR: In reading from the socket\n");
  }
  printf("%s\n", buffer);
  
  /*receive data from the server*/
  //char server_response[256];
  // recv(network_socket, &server_response, sizeof(server_response), 0);
  
  /*printing the data from the server*/
  //printf("Data from server: %s\n", server_response);

  /*closing the socket*/
  close(network_socket);

  return 0;

}
