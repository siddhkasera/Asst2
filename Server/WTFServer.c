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

int dataSize(int client_socket){
    char buffer[2];
    buffer[1] = '\0';
    int fileSize = 0;
    read(client_socket, buffer, 1);
    fileSize = atoi(buffer);

    while(1){
        if(read(client_socket, buffer, 1) < 0){
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

// Get File Data from server
char* retrieveData(int fileSize, int client_socket){
    char* fileData = malloc((fileSize+1)*sizeof(char));
    if(fileData == NULL){
        printf("ERROR: %s", strerror(errno));
        return NULL;
    }
    
    int i;
    for(i = 0; i < fileSize; i++){
        if(read(client_socket, &fileData[i], 1) < 0){
            free(fileData);
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
        }
    }
    fileData[fileSize] = '\0';
    return fileData;
}


// Read Data from File
char* readFromFile(char* filePath){
    int fd = open(filePath, O_RDONLY);
    int size = 101;
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
    if(strcmp(actions, "done") == 0 || strcmp(actions, "sending") == 0 || strcmp(actions, "ERROR") == 0) {
        return 0;
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
    return 0;
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
            if(strcmp(action, "checkout") == 0 || strcmp(action, "push") == 0 || strcmp(action, "rollback") == 0) {
                if(strcmp(action, "checkout") != 0 || strcmp(traverse -> d_name, ".data") != 0){
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
                }
            } else {
                recursiveTraverse(client_socket, dir, name, action);
            }
        } else {
            if(strcmp(action, "destroy") == 0 || strcmp(action, "rollbackDestroy") == 0){
                remove(name);
            }
            if(strcmp(action, "checkout") == 0 || strcmp(action, "rollback") == 0 || strcmp(action, "push") == 0){
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
    if(strcmp(action, "destroy") == 0 || strcmp(action, "rollbackDestroy") == 0){
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

    // Make folder for Data
    char dataPath[strlen(projectName) + 15];
    memcpy(dataPath, projectName, strlen(projectName));
    memcpy(&dataPath[strlen(projectName)], "/.data", 7);
    mkdir(dataPath, 777);

    // Make History file
    memcpy(&dataPath[strlen(projectName)], "/.data/.history", 16);
    int history = open(dataPath, O_CREAT | O_RDWR);
    chmod(dataPath, 777);
    write(history, "0\n", 2);
    close(history);

    //Manifest Stuff + Initialize Size
    write(client_socket, "sending@", 8);
    write(client_socket, "2@", 2);
    write(client_socket, "0\n", 2);
    return 0;
}

// Rollback to previous version
void rollback(int client_socket, int ver) {
    char* verString = intSpace(ver);
    char dirPath[2*strlen(projectName) + 8 + strlen(verString)];
    memcpy(dirPath, projectName, strlen(projectName));
    memcpy(&dirPath[strlen(projectName)], "/.data/", 7);
    memcpy(&dirPath[strlen(projectName) + 7], projectName, strlen(projectName) + 1);
    int exists = access(dirPath, F_OK);
    if(exists == -1){
        write(client_socket, "ERROR@", 6);
        return;
    }
    DIR* oldVer = opendir(dirPath);
    recursiveTraverse(client_socket, oldVer, dirPath, "rollback");
    char dataPath[strlen(projectName) + 8];
    DIR* currVer = opendir(projectName);
    recursiveTraverse(client_socket, currVer, projectName, "rollbackDestroy");


}

// Push Commit Changes
void push(int client_socket) {
    write(client_socket, "exists@", 7);
    int commitSize = dataSize(client_socket);
    char* commitData = retrieveData(commitSize, client_socket);

    char dirPath[strlen(projectName) + 62];
    memcpy(dirPath, projectName, strlen(projectName));
    memcpy(&dirPath[strlen(projectName)], "/.data/commit", 14);
    
    DIR* commitDir = opendir(dirPath);
    struct dirent* traverse = readdir(commitDir); 
    traverse = readdir(commitDir);
    traverse = readdir(commitDir);
    memcpy(&dirPath[strlen(projectName) + 13], "/", 1);
    while(traverse != NULL) {
        memcpy(&dirPath[strlen(projectName) + 14], traverse -> d_name, strlen(traverse -> d_name) + 1);
        char* data = readFromFile(dirPath);
        if(strcmp(data, commitData) == 0) {
            write(client_socket, "confirm@", 8);
            break;
        }
        traverse = readdir(commitDir);
    }
    closedir(commitDir);

    if(traverse == NULL){
        write(client_socket, "ERROR@", 6);
        return;
    }

    memcpy(&dirPath[strlen(projectName) + 13], "\0", 1);
    commitDir = opendir(dirPath);
    recursiveTraverse(client_socket, commitDir, dirPath, "destroy");

    DIR* workingDir = opendir(projectName);
    recursiveTraverse(client_socket, workingDir, projectName, "push");
    
    // Make Manifest Path
    char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
    memcpy(manifestPath, projectName, strlen(projectName));
    memcpy(&manifestPath[strlen(projectName)], "/", 1);
    memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);

    // Get Current Verion from Manifest File
    int fd = open(manifestPath, O_RDONLY);
    free(manifestPath);
    int ver = 0;
    char buffer[2];
    buffer[1] = '\0';
    
    int readstatus = read(fd, buffer, 1);
    if(readstatus == -1){
        printf("ERROR: %s\n", strerror(errno));
        return;
    }

    ver = atoi(buffer);
    while(1) {
        readstatus = read(fd, buffer, 1);
        if(readstatus == -1){
            printf("ERROR: %s\n", strerror(errno));
            return;
        }
        if(buffer[0] == '\n') {
            break;
        }
        ver *= 10;
        ver += atoi(buffer);
    }

    close(fd);
    char* verString = intSpace(ver);

    // Make the Name of the Project Duplicate
    char projectDup[2*strlen(projectName) + strlen(verString) + 8];
    memcpy(projectDup, projectName, strlen(projectName));
    memcpy(&projectDup[strlen(projectName)], "/.data/", 7);
    memcpy(&projectDup[strlen(projectName) + 7], projectName, strlen(projectName));
    memcpy(&projectDup[2*strlen(projectName) + 7], verString, strlen(verString) + 1);

    mkdir(projectDup, 777);

    dirPtr = subDir;
    while(dirPtr != NULL) {
        char* path = malloc((strlen(projectDup) + strlen(dirPtr -> path) - strlen(projectName) + 1)*sizeof(char));
        memcpy(path, projectDup, strlen(projectDup));
        memcpy(&path[strlen(projectDup)], &(dirPtr -> path)[strlen(projectName)], strlen(dirPtr -> path) - strlen(projectName) + 1 );
        mkdir(path, 777);
        free(path);
        dirPtr = dirPtr -> next;
    }

    filePtr = dirFiles;
    while(filePtr != NULL) {
        char* path = malloc((strlen(projectDup) + strlen(filePtr -> path) - strlen(projectName) + 1)*sizeof(char));
        memcpy(path, projectDup, strlen(projectDup));
        memcpy(&path[strlen(projectDup)], &(filePtr -> path)[strlen(projectName)], strlen(filePtr -> path) - strlen(projectName) + 1 );
        int file = open(path, O_CREAT | O_WRONLY);
        char* fileData = readFromFile(path);
        write(file, fileData, strlen(fileData));
        close(file);
        chmod(path, 777);
        free(path);
        filePtr = filePtr -> next;
    }

    int numoffiles = dataSize(client_socket);
    int i;
    for(i = 0; i < numoffiles; i++) {
        char action[2];
        read(client_socket, action, 2);
        action[1] = '\0';

        int size = 100;
        char* fileName = malloc(100*sizeof(char));
        int bytesread = 0;
        while(1) {
            read(client_socket, &fileName[bytesread], 1);
            if(fileName[bytesread] == '@') {
                fileName[bytesread] = '\0';
                break;
            }
            bytesread++;
            if(bytesread >= size-2) {
                size*=2;
                char* temp = fileName;
                fileName = malloc(size*sizeof(char));
                memcpy(fileName, temp, bytesread);
                free(temp);
            }
        }
        remove(fileName);
        if(action[0] == 'A' || action[1] == 'M') {
            int fd = open(fileName, O_CREAT | O_RDWR);   
            int fileSize = dataSize(client_socket);
            char* data = retrieveData(fileSize, client_socket);
            write(fd, data, strlen(data));
            read(client_socket, action, 1);
        }
    }

    char dataPath[strlen(projectName) + 15];
    memcpy(dataPath, projectName, strlen(projectName));
    memcpy(&dataPath[strlen(projectName)], "/.data/.history", 16);
    char* data = readFromFile(dataPath);

    int history = open(dataPath, O_CREAT | O_RDWR);
    chmod(dataPath, 777);
    write(history, data, strlen(data));
    write(history, verString, strlen(verString));
    write(history, "\n", 1);
    write(history, commitData, strlen(commitData));

    return;


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
            if(exists != -1) {
                free(projectName);
                write(client_socket, "ERROR@", 6);
                continue;
            }
            create(client_socket);
            free(projectName);
            continue;
        }

        int exists = access(projectName, F_OK);
        if(exists == -1) {
            free(projectName);
            write(client_socket, "ERROR@", 6);
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
            checkout(client_socket, projectName, actions);
            free(projectName);
            continue;
        }

        // If action is upgrade
        if(strcmp(actions, "upgrade") == 0) {
            while(strcmp(actions, "upgrade") == 0) {
                char* data = readFromFile(projectName);
                write(client_socket, "sending@", 8);
                char* number = intSpace(strlen(data));
                write(client_socket, number, strlen(number));
                write(client_socket, "@", 1);
                write(client_socket, data, strlen(data));
                readAction(client_socket);
            }
            continue;
        }

        // If action is history
        if(strcmp(actions, "history") == 0) {
            char dataPath[strlen(projectName) + 15];
            memcpy(dataPath, projectName, strlen(projectName));
            memcpy(&dataPath[strlen(projectName)], "/.data/history", 15);
            char* data = readFromFile(dataPath);
            write(client_socket, "sending@", 8);
            write(client_socket, data, strlen(data));
            write(client_socket, "@", 1);
        }

        // If action is update or currVer
        if(strcmp(actions, "update") == 0 || strcmp(actions, "currVer") == 0 ) {
            char* data = readFromFile(projectName);
            write(client_socket, "sending@", 8);
            write(client_socket, data, strlen(data));
            write(client_socket, "@", 1);
            continue;
        }

        // If action is commit
        if(strcmp(actions, "commit") == 0) {
            char* data = readFromFile(projectName);
            write(client_socket, "sending@", 8);
            write(client_socket, data, strlen(data));
            write(client_socket, "@", 1);
            readAction(client_socket);
            char dataPath[strlen(projectName) + 52];
            memcpy(dataPath, projectName, strlen(projectName) - 10);
            memcpy(&dataPath[strlen(projectName) - 10], "/.data/commit", 14);
            exists = access(dataPath, F_OK);
            if(exists == -1) {
                mkdir(dataPath, 777);
            }
            int fileSize = dataSize(client_socket);
            data = retrieveData(fileSize, client_socket);
            char hash[41];
            read(client_socket, hash, 1);
            read(client_socket, hash, 40);
            hash[40] = '\0';
            memcpy(&dataPath[strlen(projectName) - 10], "/.data/commit/.Commit", 21);
            memcpy(&dataPath[strlen(projectName) + 11], hash, 41);
            int fd = open(dataPath, O_CREAT | O_WRONLY);
            write(fd, data, strlen(data));
            continue;
        }

        // If action is rollback
        if(strcmp(actions, "rollback") == 0){
            int ver = dataSize(client_socket);
            rollback(client_socket, ver);
            continue;
        }

        // If action is push
        if(strcmp(actions, "push") == 0) {
            push(client_socket);
            continue;
        }

    }

    // Close Server Socket
    close(server_socket);
       
    return 0;
}