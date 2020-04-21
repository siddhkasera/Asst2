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


char actions[9];
char* projectName;
int server_socket;

typedef struct files{
    char* path;
    char* data;
    struct files* next;
} files;

typedef struct directories{
    char* path;
    struct directories* next;
} directories;

char actions[9];
char* projectName;
int server_socket;
files* dirFiles;
directories* subDir;
files* filePtr;
directories* dirPtr;


void intHandler(int sig_num){
    close(server_socket);
    printf("Server Closed\n");
    exit(0);
}

char* intSpace(int num) {
    int space = 0;
    int temp = num;
    if(num == 0) {
        space = 1;
    }
    while(temp != 0){
        temp/=10;
        space++;
    }
    char* string = malloc((space+1)*sizeof(char));
    sprintf(string, "%d", num);
    string[space] = '\0';
    return string;
}

// Read Data from File
char* readFromFile(char* filePath){
    int fd = open(filePath, O_RDONLY);
    int size = 100;
    char* data = malloc(101*sizeof(char));
    int bytesread = 0;
    int readstatus = 1;
    while(readstatus > 0) {
        readstatus = read(fd, &data[bytesread], 100);
        if(readstatus == -1) {
            free(data);
            close(fd);
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
        }
        bytesread += readstatus;
        if(bytesread == size - 1) {
            size *= 2;
            size--;
            char* temp = data;
            data = malloc(size*sizeof(char));
            if(data == NULL){
                free(temp);
                close(fd);
                printf("ERROR: %s\n", strerror(errno));
                return NULL;
            }
            memcpy(data, temp, bytesread + 1);
            free(temp);
            continue;
        }
        break;
    }
    data[bytesread] = '\0';
    return data;

}

// Read Action from Client
int readAction(int client_socket) {
    int bytesread = 0;
    while(bytesread < 10) {
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

// Traverse project directory
int recursiveTraverse(int client_socket, DIR* directory, char* dirPath, char* action) {;
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
            if(strcmp(action, "checkout") == 0) {
                if(subDir == NULL) {
                    subDir = malloc(sizeof(directories));
                    dirPtr = subDir;
                } else {
                    dirPtr -> next = malloc(sizeof(directories));
                    dirPtr = dirPtr -> next;
                }
                dirPtr -> path = malloc(strlen(dirPath)*sizeof(char));
                memcpy(dirPtr -> path, name, strlen(name));
                recursiveTraverse(client_socket, dir, name, action);
            } else {
                recursiveTraverse(client_socket, dir, name, action);
            }
        } else {
            if(strcmp(action, "destroy") == 0){
                remove(name);
            }
            if(strcmp(action, "checkout") == 0){
                if(filePtr == NULL) {
                    dirFiles = malloc(sizeof(files));
                    filePtr = dirFiles;
                }
                else {
                    filePtr -> next = malloc(sizeof(files));
                    filePtr = filePtr -> next;
                }
                filePtr -> path = malloc((strlen(name) +1)*sizeof(char));
                memcpy(filePtr -> path, name, strlen(name) + 1);
                filePtr -> data = readFromFile(name);
            }
        }
        traverse = readdir(directory);
        free(name);
    }
    if(strcmp(action, "destroy") == 0){
        rmdir(dirPath);
    }
    closedir(directory);
    return 0;
}

// Sends a copy of a Project Directory to the Client 
void checkout(int client_socket, char* projectName, char* action) {
    DIR* directory = opendir(projectName);
    recursiveTraverse(client_socket, directory, projectName, action);

    // Find Num of Directories
    int dirNum = 0;
    directories* ptr = subDir;
    while(ptr != NULL) {
        dirNum++;
        ptr = ptr -> next;
    }
    char* numOfDir = intSpace(dirNum);

    // Write to Client Num of Directories
    write(client_socket, "sending@", 8);
    write(client_socket, numOfDir, strlen(numOfDir));
    write(client_socket, "@", 1);

    // Write to Client Directory Names
    int i;
    directories* dptr = subDir;
    for(i = 0; i < dirNum; i++) {
        write(client_socket, dptr -> path, strlen(dptr -> path));
        write(client_socket, "@", 1);
        dptr = dptr -> next;
    }

    // Find Num of Files
    int fileNum = 0;
    files* fptr = dirFiles;
    while(fptr != NULL) {
        fileNum++;
        fptr = fptr -> next;
    }
    char* numOfFile = intSpace(fileNum); 

    // Write to Client Num of Files
    write(client_socket, numOfFile, strlen(numOfFile));
    write(client_socket, "@", 1);

    // Write to Client File Names and Data
    fptr = dirFiles;
    for(i = 0; i < fileNum; i++) {
        write(client_socket, fptr -> path, strlen(fptr -> path));
        write(client_socket, "@", 1);
        char* data = readFromFile(fptr -> path);
        char* len = intSpace(strlen(data));
        write(client_socket, len, strlen(len));
        write(client_socket, "@", 1);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
        fptr = fptr -> next;
    }

}

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
    write(fd, "0\n", 2);
    close(fd);

    //Manifest Stuff + Initialize Size
    write(client_socket, "sending@", 8);
    write(client_socket, "2@", 2);
    write(client_socket, "0\n", 2);
    return 0;
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
            recursiveTraverse(client_socket, directory, projectName, actions);
            free(projectName);
            write(client_socket, "success@", 8);
            continue;
        }

        // If action is checkout
        if(strcmp(actions, "checkout") == 0) {
            int exists = access(projectName, F_OK);
            if(exists == -1) {
                free(projectName);
                write(client_socket, "ERROR@", 6);
                continue;
            }
            checkout(client_socket, projectName, actions);
            free(projectName);
            continue;
        }
    }

    // Close Server Socket
    close(server_socket);
       
    return 0;
}