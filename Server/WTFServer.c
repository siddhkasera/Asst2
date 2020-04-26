#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>


char actions[8];
char* projectName;
int server_socket;

int version;



void intHandler(int sig_num){
    close(server_socket);
    printf("Server Closed\n");
    exit(0);
}

int readAction(int client_socket) {
    int bytesread = 0;
    while(bytesread < 9) {
        if(read(client_socket, &actions[bytesread], 1) < 0){
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        if(actions[bytesread] == '@'){
            actions[bytesread] = '\0';
            break;
        }
        bytesread++;
    }

    bytesread = 0;
    int size = 100;
    int readstatus = 1;
    projectName = malloc(100*sizeof(char));

    while(1){
        if(read(client_socket, &projectName[bytesread], 1) < 0){
            free(projectName);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        if(projectName[bytesread] == '@') {
            projectName[bytesread] = '\0';
            break;
        }
        bytesread++;
        if(bytesread >= size -2) {
            char* temp = projectName;
            size = size*=2;
            projectName = malloc(size*sizeof(char));
            if(projectName == NULL){
                free(temp);
                printf("ERROR: %s", strerror(errno));
                return -1;
            }
            memcpy(projectName, temp, bytesread);
            free(temp);
        }
    }
}


int create(int client_socket) {
    mkdir(projectName, 777);
    char filePath[2*strlen(projectName) + 11];
    char * manifest = ".manifest";
    memcpy(filePath, projectName, strlen(projectName));
    memcpy(&filePath[strlen(projectName)], "/", 1);
    memcpy(&filePath[strlen(projectName) + 1], projectName, strlen(projectName));
    memcpy(&filePath[2*strlen(projectName) + 1], manifest, 10);

// Creates a Project Directory and Initializes a .manifest file
int create(int client_socket) {
    // Make the new Project Directory
    mkdir(projectName, 777);

    // Make the Manifest File
    char filePath[strlen(projectName) + 11];
    memcpy(filePath, projectName, strlen(projectName));
    memcpy(&filePath[strlen(projectName)], "/", 1);
    memcpy(&filePath[strlen(projectName) + 1], ".manifest", 10);

    int fd = open(filePath, O_RDWR | O_CREAT, 777);
    if(fd == -1){
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }


    char space = ' ';
    write(fd, version, sizeof(int));
    write(fd, space, 1);
    write(fd, filePath, sizeof(filePath));
    write(fd, space, 1);
    
    //Manifest Stuff + Initialize Size
    /* write(client_socket, "sending@", 8);
    write(client_socket, "5@", 2);
    write(client_socket, "hello", 5);
    */
    return 0;
}


    //Manifest Stuff + Initialize Size
    write(client_socket, "sending@", 8);
    write(client_socket, "2@", 2);
    write(client_socket, "0\n", 2);
    return 0;
}

// Recursively deletes a project directory

int destroy(int client_socket, DIR* directory, char* dirPath) {
    struct dirent* traverse = readdir(directory);
    traverse = readdir(directory);
    traverse = readdir(directory);
    char * name;
    while(traverse != NULL){
        name = malloc((strlen(traverse -> d_name)+ strlen(dirPath) + 2)*sizeof(char));
        if(name == NULL){ 
            printf("Error: %s\n", strerror(errno));
            closedir(directory);
            return -1;
        }
        memcpy(name, dirPath, strlen(dirPath));
        memcpy(name + strlen(dirPath), "/", 1);
        memcpy(name + strlen(dirPath) + 1, traverse -> d_name, strlen(traverse -> d_name) + 1);
        if(traverse -> d_type == DT_DIR){ //Checks if its a directory
            DIR* dir = opendir(name);
            if(dir == NULL){ //Failed to Open Directory
                printf("Error: %s\n", strerror(errno));
                return -1;
            }
            destroy(client_socket, dir, name);
        } else {
            remove(name);
        }
        traverse = readdir(directory);
        free(name);
    }
    rmdir(dirPath);
    closedir(directory);
    return 0;
}
int datasize(){
  char buffer[2];
  buffer[1] = '\0';
  int fileSize = 0;
  read(server_socket, buffer, 1);
  fileSize = atoi(buffer);

  while(1){
    if(read(server_socket, buffer, 1) < 0){
      cleanUp();
      printf("ERROR: %s\n", strerror(errno));
      return -1;
    }
    if(buffer[0] == '@') {
      break;
    }
    fileSize *= 10;
    fileSize += atoi (buffer);
  }

  return fileSize;

}
int push( int client_socket, char * commitPath, char * projectName, char * action){
  DIR * oDirectory = opendir(projectName);
  recursiveTraverse(int client_socket, directory, projectName, action
  //reading the server's commit file
  char * commitFile = readFromFile(commitPath);
  
  int size = 100;
  char * clientCommitFile = malloc(size * sizeof(char));

  if(clientCommitFile == NULL){
    prinft("ERROR: %s\n", strerror(errno));
    return -1;
  }
  //reading part
  int readstatus = 1;
  int bytesread = 0;
  while(readstatus > 1){
    if(bytesread >=2){
      char * temp = clientCommitFile;
      size *=2;
      clientCommitFile = malloc(size * sizeof(char));
      if(clientCommitFile == NULL){
	printf("ERROR: %s\n", strerror(errno));
	return -1;
      }
      memcpy(clientCommitFile, temp, bytesread);
      free(temp);
    }
    readstatus = read(server_socket, clientCommitFile+bytesread, 1);
    if(readstatus == -1){
      printf("ERROR: %s\n", strerror(errno));
      free(clientCommitFile);
      return -1;
    }
    //comparing the two commitFiles                                             
    if(strcmp(commitFile, clientCommitFile ) == 0){
      write(server_socket, "confirmed@", 10);
    }

    directories * dPtr = subDir;
    while(dPtr != NULL){
      char * dirName = malloc(strlen(dPtr->path)+ 1 * sizeof(char));
      //char * dirPath = malloc(strlen(dPtr->path)+ 1 * sizeof(char));
      memcpy(dirName, projectName0, strlen(projectName0));
      memcpy(dirName[strlen(projectName0)+1], "/",1);
      memcpy(dirName[strlen(projectName0)+1], directory, strlen(directory));
      mkdir(dirName, 777);
    }
      
    
      files * filePtr = dirFiles;
      while(filePtr != NULL){
      char * fileName = malloc(strlen(filePtr->path) * sizeof(char));
      memcpy(fileName, projectName0, strlen(projectName));
      memcpy(fileName[strlen(projectName0) + 1], "/", 1);
      memcpy(fileName[strlen(projectName0) + 1], fileName, strlen(fileName));
      int fd = open(fileName, O_CREAT | O_RDWR);
      chmod(fileName, 777);
      char * data = readFromFile(filePtr -> path);
      write(fd, data, sizeof(data));
 
    // Read clients response back for confirmed@
      while(bytesread < 13) {
      if(read(server_socket, &actions[bytesread], 1) < 0){
	cleanUp();
	printf("ERROR: %s\n", strerror(errno));
	return -1;
      }
      if(actions[bytesread] == '@'){
	actions[bytesread] = '\0';
	break;
      }
      bytesread++;
    }

    if( strcmp(actions, "ERROR") == 0) {
      printf("ERROR\n");
      return -1;
    }
    //reading the no of files
    int numoffiles;
    char buffer[2];
    buffer[1] = '\0';
    
    //read from client number of files
    numoffiles = dataSize();
    
    char status[2];
    int readstatus1 = 1;
    readstatus1 = read(server_socket, buffer, 1);
    if(readstatus1 == -1){
      printf("ERROR: %s\n", strerror(errno));
    }
    buffer[0] = '\0';
    
    int i;
    for(i =0; i<numoffiles;i++){
      int size = 100;
      char * filename = malloc(100 * sizeof(char));
      bytesread = 0;
      while(1){
	read(server_socket, &filename[bytesread], 1);
	if(filename[bytesread] == '@'){
	  filename[bytesread] = '\0';
	  break;
	}
	bytesread++;
	if(bytesread >= size -2){
	  size *=2;
	  char * temp = filename;
	  filename =  malloc(size * sizeof(char));
	  memcpy(filename, temp, bytesread);
	  free(temp);
	}
      }
      //create and open the file sent by the client
      int fd = open(filename, O_CREAT | O_RDWR);
      if(fd == -1){
	printf("ERROR: %s\n", strerror(errno));
	return -1;
      }
      char buffer[2];
      int filesize = 0;
      int readstatus = 1;
      buffer[1] = '\0';
      readstatus = read(server_socket, buffer, 1);
      if(readstatus == -1){
	printf("ERROR: %s\n", strerror(errno));
      }
      filesize = atoi(buffer);
      
      char* fileData = malloc(fileSize*sizeof(char));
      if(fileData == NULL){
	cleanUp();
	printf("ERROR: %s", strerror(errno));
	return -1;
      }
                
      // Read in File Data
      int i;
      for(i = 0; i < fileSize; i++){
	if(read(server_socket, &fileData[i], 1) < 0){
	  free(fileData);
	  cleanUp();
	  printf("ERROR: %s\n", strerror(errno));
	  return NULL;
	}
      }
      fileData[fileSize] = '\0';
      int wt = write(fd, fileData, sizeof(fileData));
      if(wt == NULL){
	printf("ERROR: %s\n", strerror(errno));
	return -1;
      }
      int manifestFile = open(manifestPath, O_RDWR);
   
    }
    

	  

int main(int argc, char* argv[]) {
    if(argc != 2) {
        printf("ERROR: Incorrect Number of Arguments\n");
        return -1;
    }
    char server_message[256] = "You have reached the server";

    // Create Server Socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0){
        printf("ERROR: In creating the socket\n");
        return -1;
    }

    // Define the Server Address
    struct sockaddr_in server_address;
    bzero((char*) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[1]));
    server_address.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to specified IP and port
    if(bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
        printf("ERROR: In binding to the socket\n");
        return -1;
    }
    // Listen to connections
    listen(server_socket, 5);
    signal(SIGINT, intHandler);

    // Accept Connections 
    while(1) {
        int client_socket;
        client_socket = accept(server_socket, NULL, NULL);

        readAction(client_socket);

        // If action is create

        if(strcmp(actions, "create") == 0){
            int exists = access(projectName, F_OK);
            if(exists == 0) {
                write(client_socket, "ERROR@", 6);
                free(projectName);
                continue;
            }
            create(client_socket);
            free(projectName);
            continue;
        }



        // If action is destroy
        if(strcmp(actions, "destroy") == 0){
            int exists = access(projectName, F_OK);
            if(exists == -1) {
                free(projectName);
                write(client_socket, "ERROR@", 6);
                continue;
            }
            DIR* directory = opendir(projectName);
            destroy(client_socket, directory, projectName);
            free(projectName);
            write(client_socket, "success@", 8);
            continue;
        }
	if(strcmp(actions, "update") == 0) {
	  int fd = access(projectName, O_RDONLY);
	  char* data = readFromFile(projectName);
	  write(client_socket, "sending@", 8);
	  char* number = intSpace(strlen(data));
	  write(client_socket, number, strlen(number));
	  write(client_socket, "@", 1);
	  write(client_socket, data, strlen(data));
	  write(client_socket, "@", 1);
	  continue;
        }

    }

    // Close Server Socket
    close(server_socket);
       
    return 0;

}

}

