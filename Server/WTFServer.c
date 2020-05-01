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
#include <pthread.h>

typedef struct files{
    char* path;
    char* data;
    struct files* next;
} files;

typedef struct projectMutexes {
    pthread_mutex_t * mutex;
    char* project;
    struct projectMutexes* next;
} projectMutexes;

typedef struct directories{
    char* path;
    struct directories* next;
} directories;

projectMutexes* mutexes = NULL;
projectMutexes* mutPtr = NULL;
int server_socket;
files* dirFiles = NULL;
directories* subDir = NULL;
files* filePtr = NULL;
directories* dirPtr = NULL;
pthread_mutex_t* createMut;


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
    if(string == NULL) {
        printf("ERROR: %s\n", strerror(errno));
        return NULL;
    }
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
    if(fd == -1){
        printf("ERROR: Could not open %s\n", filePath);
        return NULL;
    }
    int size = 101;
    char* data = malloc(101*sizeof(char));
    if(data == NULL) {
        printf("ERROR: %s\n", strerror(errno));
        close(fd);
        return NULL;
    }
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
        if(bytesread < 100) {
            break;
        }
    }
    data[bytesread] = '\0';
    return data;

}

// Read Action from Client
char* readAction(int client_socket) {
    int bytesread = 0;
    char* actions = malloc(9*sizeof(char));
    if(actions == NULL) {
        printf("ERROR: %s\n", strerror(errno));
        return NULL;
    }
    while(bytesread < 10) {
        if(read(client_socket, &actions[bytesread], 1) < 0){
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
        }
        if(actions[bytesread] == '@'){
            actions[bytesread] = '\0';
            break;
        }
        bytesread++;
    }
    return actions;
}

// Read in Project Name
char* readProjectName(int client_socket) {
    int bytesread = 0;
    int size = 100;
    int readstatus = 1;
    char* projectName = malloc(100*sizeof(char));
    if(projectName == NULL) {
        printf("ERROR: %s\n", strerror(errno));
        return NULL;
    }
    while(1){
        if(read(client_socket, &projectName[bytesread], 1) < 0){
            free(projectName);
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
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
                return NULL;
            }
            memcpy(projectName, temp, bytesread);
            free(temp);
        }
    }
    return projectName;
}

// Traverse project directory
int recursiveTraverse(DIR* directory, char* dirPath, char* action, char* projectName) {;
    struct dirent* traverse = readdir(directory);
    traverse = readdir(directory);
    traverse = readdir(directory);
    char * name;
    while(traverse != NULL){
        name = malloc((strlen(traverse -> d_name)+ strlen(dirPath) + 2)*sizeof(char));
        if(name == NULL){ 
            printf("ERROR: %s\n", strerror(errno));
            closedir(directory);
            return -1;
        }
        memcpy(name, dirPath, strlen(dirPath));
        memcpy(name + strlen(dirPath), "/", 1);
        memcpy(name + strlen(dirPath) + 1, traverse -> d_name, strlen(traverse -> d_name) + 1);
        if(traverse -> d_type == DT_DIR){ //Checks if its a directory
            DIR* dir = opendir(name);
            if(dir == NULL){ //Failed to Open Directory
                printf("ERROR: %s\n", strerror(errno));
                return -1;
            }
            if(strcmp(action, "mutex") == 0) {
                if(mutexes == NULL) {
                    mutexes = malloc(sizeof(projectMutexes));
                    mutPtr = mutexes;
                } else {
                    mutPtr -> next = malloc(sizeof(projectMutexes));
                    mutPtr = mutPtr -> next;
                }
                mutPtr -> next = NULL;
                mutPtr -> project = malloc((strlen(traverse -> d_name) + 1)*sizeof(char));
                mutPtr -> mutex = malloc(sizeof(pthread_mutex_t)); 
                pthread_mutex_init(mutPtr -> mutex, NULL);
                memcpy(mutPtr -> project, traverse -> d_name, strlen(traverse -> d_name) + 1);
            }
            else if(strcmp(action, "checkout") == 0 || strcmp(action, "push") == 0 || strcmp(action, "rollback") == 0) {
                if(strcmp(traverse -> d_name, ".data") != 0){
                    if(subDir == NULL) {
                        subDir = malloc(sizeof(directories));
                        dirPtr = subDir;
                    } else {
                        dirPtr -> next = malloc(sizeof(directories));
                        dirPtr = dirPtr -> next;
                    }
                    dirPtr -> next = NULL;
                    dirPtr -> path = malloc((strlen(name) + 1)*sizeof(char));
                    memcpy(dirPtr -> path, name, strlen(name) + 1);
                    recursiveTraverse(dir, name, action, projectName);
                }
            } else {
                if(strcmp(action, "rollbackDestroy") != 0 || strcmp(traverse -> d_name, ".data") != 0) {
                    recursiveTraverse(dir, name, action, projectName);
                }
            }
        } else {
            if(strcmp(action, "destroy") == 0 || strcmp(action, "rollbackDestroy") == 0){
                remove(name);
            }
            if(strcmp(action, "checkout") == 0 || strcmp(action, "rollback") == 0 || strcmp(action, "push") == 0){
                if(strcmp(action, "checkout") != 0 || strcmp(traverse -> d_name, ".history") != 0) {
                    if(filePtr == NULL) {
                        dirFiles = malloc(sizeof(files));
                        filePtr = dirFiles;
                    }
                    else {
                        filePtr -> next = malloc(sizeof(files));
                        filePtr = filePtr -> next;
                    }
                    filePtr -> next = NULL;
                    filePtr -> path = malloc((strlen(name) +1)*sizeof(char));
                    memcpy(filePtr -> path, name, strlen(name) + 1);
                    filePtr -> data = readFromFile(name);
                }
            }
        }
        traverse = readdir(directory);
        free(name);
    }
    if(strcmp(action, "destroy") == 0 || strcmp(action, "rollbackDestroy") == 0){
        if(strcmp(action, "rollbackDestroy") != 0 || strcmp(dirPath, projectName) != 0) {
            rmdir(dirPath);
        }
    }
    closedir(directory);
    return 0;
}

// Sends a copy of a Project Directory to the Client 
int checkout(int client_socket, char* projectName, char* action) {
    DIR* directory = opendir(projectName);
    int status = recursiveTraverse(directory, projectName, action, projectName);
    if(status == -1) {
        return -1;
    }

    // Find Num of Directories
    int dirNum = 0;
    directories* ptr = subDir;
    while(ptr != NULL) {
        dirNum++;
        ptr = ptr -> next;
    }
    char* numOfDir = intSpace(dirNum);
    if(numOfDir == NULL) {
        return -1;
    }

    // Write to Client Num of Directories
    write(client_socket, "sending@", 8);
    write(client_socket, numOfDir, strlen(numOfDir));
    write(client_socket, "@", 1);
    free(numOfDir);

    // Write to Client Directory Names
    int i;
    directories* dptr = subDir;
    for(i = 0; i < dirNum; i++) {
        write(client_socket, dptr -> path, strlen(dptr -> path));
        write(client_socket, "@", 1);
        dptr = dptr -> next;
        char* response = readAction(client_socket);
        if(strcmp(response, "recieved") != 0) {
            free(response);
            return -1;
        }
        free(response);
    }

    // Find Num of Files
    int fileNum = 0;
    files* fptr = dirFiles;
    while(fptr != NULL) {
        fileNum++;
        fptr = fptr -> next;
    }
    char* numOfFile = intSpace(fileNum); 
    if(numOfFile == NULL){
        return -1;
    }

    // Write to Client Num of Files
    write(client_socket, numOfFile, strlen(numOfFile));
    write(client_socket, "@", 1);
    free(numOfFile);

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
        char* response = readAction(client_socket);
        if(strcmp(response, "recieved") != 0) {
            free(response);
            return -1;
        }
        free(response);
        fptr = fptr -> next;
    }

}

// Creates a Project Directory and Initializes a .manifest file
int create(int client_socket, char* projectName) {
    // Make the new Project Directory
    mkdir(projectName, 0777);

    // Make the Manifest File
    char filePath[strlen(projectName) + 11];
    memcpy(filePath, projectName, strlen(projectName));
    memcpy(&filePath[strlen(projectName)], "/", 1);
    memcpy(&filePath[strlen(projectName) + 1], ".manifest", 10);
    int fd = open(filePath, O_RDWR | O_CREAT, 0777);
    if(fd == -1){
        printf("ERROR: Could not open .manifest file\n");
        return -1;
    }
    write(fd, "0\n", 2);
    close(fd);

    // Make folder for Data
    char dataPath[strlen(projectName) + 15];
    memcpy(dataPath, projectName, strlen(projectName));
    memcpy(&dataPath[strlen(projectName)], "/.data", 7);
    mkdir(dataPath, 0777);

    // Make History file
    memcpy(&dataPath[strlen(projectName)], "/.history", 10);
    int history = open(dataPath, O_CREAT | O_RDWR, 0777);
    write(history, "0\n", 2);
    close(history);

    //Manifest Stuff + Initialize Size
    write(client_socket, "sending@", 8);
    write(client_socket, "2@", 2);
    write(client_socket, "0\n", 2);
    char* response = readAction(client_socket);
    if(response != NULL) {
        free(response);
        return 0;
    } else {
        printf("ERROR: Could not recieve confirmation from client\n");
        return -1;
    }
}

// Rollback to previous version
void rollback(int client_socket, int ver, char* projectName) {
    char* verString = intSpace(ver);
    char dirPath[2*strlen(projectName) + 8 + strlen(verString)];
    memcpy(dirPath, projectName, strlen(projectName));
    memcpy(&dirPath[strlen(projectName)], "/.data/", 7);
    memcpy(&dirPath[strlen(projectName) + 7], projectName, strlen(projectName));
    memcpy(&dirPath[2*strlen(projectName) + 7], verString, strlen(verString) + 1);

    int exists = access(dirPath, F_OK);
    if(exists == -1){
        write(client_socket, "ERROR@", 6);
        return;
    }

    DIR* oldVer = opendir(dirPath);
    recursiveTraverse(oldVer, dirPath, "rollback", projectName);
    DIR* currVer = opendir(projectName);
    recursiveTraverse(currVer, projectName, "rollbackDestroy", projectName);

    dirPtr = subDir;
    while(dirPtr != NULL) {
        char* dirname = strstr(dirPtr -> path, &dirPath[strlen(projectName) + 7]);
        char path[strlen(projectName) + strlen(&dirname[strlen(projectName)]) + 1];
        memcpy(path, projectName, strlen(projectName));
        memcpy(&path[strlen(projectName)], &dirname[strlen(projectName) + 1], strlen(&dirname[strlen(projectName)]));
        mkdir(path, 0777);
        dirPtr = dirPtr -> next;
    }

    filePtr = dirFiles;
    while(filePtr != NULL) {
        char* filename = strstr(filePtr -> path, &dirPath[strlen(projectName) + 7]);
        char path[strlen(projectName) + strlen(&filename[strlen(projectName)]) + 1];
        memcpy(path, projectName, strlen(projectName));
        memcpy(&path[strlen(projectName)], &filename[strlen(projectName) + 1], strlen(&filename[strlen(projectName)]));
        int file = open(path, O_CREAT | O_WRONLY, 0777);
        char* fileData = readFromFile(filePtr -> path);
        write(file, fileData, strlen(fileData));
        free(fileData);
        close(file);
        filePtr = filePtr -> next;
    }

    memcpy(&dirPath[strlen(projectName) + 6], "\0", 1);

    DIR* dataDir = opendir(dirPath);
    struct dirent* traverse = readdir(dataDir);
    traverse = readdir(dataDir);
    traverse = readdir(dataDir);
    while(traverse != NULL) {
        int dirVer = atoi(&(traverse ->d_name)[strlen(projectName)]);
        if(dirVer >= ver){
            char verPath[strlen(traverse -> d_name) + strlen(dirPath) + 2];
            memcpy(verPath, dirPath, strlen(dirPath));
            memcpy(&verPath[strlen(dirPath)], "/", 1);
            memcpy(&verPath[strlen(dirPath) + 1], traverse -> d_name, strlen(traverse -> d_name));
            verPath[strlen(dirPath) + strlen(traverse -> d_name) + 1] = '\0';
            recursiveTraverse(opendir(verPath), verPath, "destroy", projectName);
        }
        traverse = readdir(dataDir);
    }
    write(client_socket, "complete@", 9);

}

// Push Commit Changes
int push(int client_socket, char* projectName) {
    write(client_socket, "exists@", 7);
    int commitSize = dataSize(client_socket);
    if(commitSize == -1) {
        return -1;
    }
    char* commitData = retrieveData(commitSize, client_socket);
    if(commitData == NULL) {
        return -1;
    }

    char dirPath[strlen(projectName) + 62];
    memcpy(dirPath, projectName, strlen(projectName));
    memcpy(&dirPath[strlen(projectName)], "/.data/commit", 14);
    if(access(dirPath, F_OK) == -1) {
        free(commitData);
        write(client_socket, "ERROR@", 6);
        return -1;
    }
    DIR* commitDir = opendir(dirPath);
    struct dirent* traverse = readdir(commitDir); 
    traverse = readdir(commitDir);
    traverse = readdir(commitDir);
    memcpy(&dirPath[strlen(projectName) + 13], "/", 1);
    while(traverse != NULL) {
        memcpy(&dirPath[strlen(projectName) + 14], traverse -> d_name, strlen(traverse -> d_name) + 1);
        char* data = readFromFile(dirPath);
        if(data == NULL){
            printf("ERROR: %s\n", strerror(errno));
            free(commitData);
            return -1;
        }
        if(strcmp(data, commitData) == 0) {
            write(client_socket, "confirm@", 8);
            free(data);
            break;
        }
        free(data);
        traverse = readdir(commitDir);
    }
    closedir(commitDir);

    if(traverse == NULL){
        write(client_socket, "ERROR@", 6);
        free(commitData);
        return -1;
    }

    memcpy(&dirPath[strlen(projectName) + 13], "\0", 1);
    commitDir = opendir(dirPath);
    recursiveTraverse(commitDir, dirPath, "destroy", projectName);

    DIR* workingDir = opendir(projectName);
    recursiveTraverse(workingDir, projectName, "push", projectName);
    
    // Make Manifest Path
    char manifestPath[strlen(projectName) + 11];
    memcpy(manifestPath, projectName, strlen(projectName));
    memcpy(&manifestPath[strlen(projectName)], "/", 1);
    memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);

    // Get Current Verion from Manifest File
    int fd = open(manifestPath, O_RDONLY);
    if(fd == -1) {
        printf("ERROR: Could not open .manifest file\n");
        return -1;
    }

    int ver = 0;
    char buffer[2];
    buffer[1] = '\0';
    
    int readstatus = read(fd, buffer, 1);
    if(readstatus == -1){
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    ver = atoi(buffer);
    while(1) {
        readstatus = read(fd, buffer, 1);
        if(readstatus == -1){
            printf("ERROR: %s\n", strerror(errno));
            return -1;
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

    mkdir(projectDup, 0777);

    dirPtr = subDir;
    while(dirPtr != NULL) {
        char path[strlen(projectDup) + strlen(dirPtr -> path) - strlen(projectName) + 1];
        memcpy(path, projectDup, strlen(projectDup));
        memcpy(&path[strlen(projectDup)], &(dirPtr -> path)[strlen(projectName)], strlen(dirPtr -> path) - strlen(projectName) + 1 );
        mkdir(path, 0777);
        dirPtr = dirPtr -> next;
    }

    filePtr = dirFiles;
    while(filePtr != NULL) {
        char path[strlen(projectDup) + strlen(filePtr -> path) - strlen(projectName) + 1];
        memcpy(path, projectDup, strlen(projectDup));
        memcpy(&path[strlen(projectDup)], &(filePtr -> path)[strlen(projectName)], strlen(filePtr -> path) - strlen(projectName) + 1 );
        int file = open(path, O_CREAT | O_WRONLY, 0777);
        if(file == -1) {
            printf("ERROR: Could not create %s\n", path);
            return -1;
        }
        char* fileData = readFromFile(filePtr -> path);
        write(file, fileData, strlen(fileData));
        free(fileData);
        close(file);
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
        if(action[0] == 'A' || action[0] == 'M') {
            int start = strlen(projectName) + 1;
            char* filepath = strstr(&fileName[start], "/");
            while(filepath != NULL) {
                int size = 0;
                while(fileName[start + size] != '/') {
                    size++;
                }
                char dirname[start + size + 1];
                memcpy(dirname, fileName, start + size);
                dirname[start+size] = '\0';
                mkdir(dirname, 0777);
                start += size + 1;
                filepath = strstr(&fileName[start], "/");
            }
            int fd = open(fileName, O_CREAT | O_RDWR, 0777);   
            int fileSize = dataSize(client_socket);
            if(fileSize != 0) {
                char* data = retrieveData(fileSize, client_socket);
                write(fd, data, strlen(data));
            }
            close(fd);
            read(client_socket, action, 1);
        }
    }

    remove(manifestPath);
    int manifest = open(manifestPath, O_CREAT | O_RDWR, 0777);
    int manifestSize = dataSize(client_socket);
    char* manifestData = retrieveData(manifestSize, client_socket);
    write(manifest, manifestData, strlen(manifestData));
    close(manifest);

    int newVer = atoi(verString) + 1;
    verString = intSpace(newVer);
    char dataPath[strlen(projectName) + 10];
    memcpy(dataPath, projectName, strlen(projectName));
    memcpy(&dataPath[strlen(projectName)], "/.history", 10);
    char* data = readFromFile(dataPath);

    int history = open(dataPath, O_CREAT | O_RDWR, 0777);
    write(history, data, strlen(data));
    write(history, "\n", 1);
    write(history, verString, strlen(verString));
    write(history, "\n", 1);
    write(history, commitData, strlen(commitData));

}

void * connection_handler(void * p_client_socket){

    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    char* actions = readAction(client_socket);
    char* projectName = readProjectName(client_socket);
    pthread_mutex_t * projMut = NULL;

    // If action is create
    if(strcmp(actions, "create") == 0) {
        pthread_mutex_lock(createMut);
        int exists = access(projectName, F_OK);
        if(exists != -1) {
            free(projectName);
            write(client_socket, "ERROR@", 6);
            printf("Disconnected from Client\n");
            pthread_exit(NULL);
        }
        int mutExists = 0;
        projectMutexes* createPtr = mutexes;
        projectMutexes* prev = NULL;
        while(createPtr != NULL) {
            if(strcmp(createPtr -> project, projectName) == 0) {
                projMut = createPtr -> mutex;
                pthread_mutex_lock(projMut);
                mutExists = 1;
                break;
            }
            prev = createPtr;
            createPtr = createPtr -> next;
        }
        if(mutExists == 0) {
            projectMutexes* newProj = malloc(sizeof(projectMutexes));
            newProj -> project = malloc((strlen(projectName) + 1)*sizeof(char));
            memcpy(newProj -> project, projectName, strlen(projectName) + 1);
            newProj -> mutex = malloc(sizeof(pthread_mutex_t)); 
            pthread_mutex_init(newProj -> mutex, NULL);
            projMut = newProj -> mutex;
            pthread_mutex_lock(newProj -> mutex);
            newProj -> next = NULL;
            if(prev == NULL){
                mutexes = newProj;
            } else {
                prev -> next = newProj;
            }
        }
        create(client_socket, projectName);
        free(projectName);
        pthread_mutex_unlock(projMut);
        pthread_mutex_unlock(createMut);
        printf("Disconnected from Client\n");
        pthread_exit(NULL);
    }

    projectMutexes* ptr = mutexes;
    while(ptr != NULL) {
        if(strcmp(ptr -> project, projectName) == 0) {
            projMut = ptr -> mutex;
            pthread_mutex_lock(projMut);
            break;
        }
        ptr = ptr -> next;
    }

    int exists = access(projectName, F_OK);
    if(exists == -1) {
        free(projectName);
        write(client_socket, "ERROR@", 6);
        if(projMut != NULL){
            pthread_mutex_unlock(projMut);
        }
        printf("Disconnected from Client\n");
        pthread_exit(NULL);
    }

    // If action is destroy
    if(strcmp(actions, "destroy") == 0){
        DIR* directory = opendir(projectName);
        recursiveTraverse(directory, projectName, actions, projectName);
        free(projectName);
        write(client_socket, "success@", 8);
        char* response = readAction(client_socket);
        if(strcmp(response, "recieved") != 0) {
            printf("ERROR: Message not recieved by Client\n");  
        }
    }

    // If action is checkout
    else if(strcmp(actions, "checkout") == 0) {
        checkout(client_socket, projectName, actions);
        free(projectName);
    }

    // If action is upgrade
    else if(strcmp(actions, "upgrade") == 0) {
        write(client_socket, "exists@", 8);
        while(strcmp(actions, "upgrade") == 0) {
            actions = readAction(client_socket);
            if(strcmp(actions, "done") == 0){
                break;
            }
            projectName = readProjectName(client_socket);
            char* data = readFromFile(projectName);
            write(client_socket, "sending@", 8);
            char* number = intSpace(strlen(data));
            write(client_socket, number, strlen(number));
            write(client_socket, "@", 1);
            write(client_socket, data, strlen(data));
        }
        projectName = readProjectName(client_socket);
        // Make Manifest Path
        char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
        memcpy(manifestPath, projectName, strlen(projectName));
        memcpy(&manifestPath[strlen(projectName)], "/", 1);
        memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);

        // Get Current Verion from Manifest File
        int fd = open(manifestPath, O_RDONLY);
        int ver = 0;
        char buffer[2];
        buffer[1] = '\0';
        
        int readstatus = read(fd, buffer, 1);
        if(readstatus == -1){
            printf("ERROR: %s\n", strerror(errno));
            printf("Disconnected from Client\n");
            pthread_exit(NULL);
        }

        ver = atoi(buffer);
        while(1) {
            readstatus = read(fd, buffer, 1);
            if(readstatus == -1){
                printf("ERROR: %s\n", strerror(errno));
                printf("Disconnected from Client\n");
                pthread_exit(NULL);
            }
            if(buffer[0] == '\n') {
                break;
            }
            ver *= 10;
            ver += atoi(buffer);
        }

        close(fd);
        char* verString = intSpace(ver);
        char* verSpace = intSpace(strlen(verString));
        write(client_socket, verSpace, strlen(verSpace));
        write(client_socket, "@", 1);
        write(client_socket, verString, strlen(verString));
        write(client_socket, "@", 1);
        
    }

    // If action is history
    else if(strcmp(actions, "history") == 0) {
        char dataPath[strlen(projectName) + 10];
        memcpy(dataPath, projectName, strlen(projectName));
        memcpy(&dataPath[strlen(projectName)], "/.history", 10);
        char* data = readFromFile(dataPath);
        write(client_socket, "sending@", 8);
        char* space = intSpace(strlen(data));
        write(client_socket, space, strlen(space));
        write(client_socket, "@", 1);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
    }

    // If action is update or currVer
    else if(strcmp(actions, "update") == 0 || strcmp(actions, "currVer") == 0) {
        char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
        memcpy(manifestPath, projectName, strlen(projectName));
        memcpy(&manifestPath[strlen(projectName)], "/", 1);
        memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);
        char* data = readFromFile(manifestPath);
        write(client_socket, "sending@", 8);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
    }


    // If action is commit
    else if(strcmp(actions, "commit") == 0) {
        char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
        memcpy(manifestPath, projectName, strlen(projectName));
        memcpy(&manifestPath[strlen(projectName)], "/", 1);
        memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);
        char* data = readFromFile(manifestPath);
        free(manifestPath);
        write(client_socket, "sending@", 8);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
        actions = readAction(client_socket);
        if(strcmp(actions, "ERROR") == 0) {
            pthread_mutex_unlock(projMut);
            printf("Disconnected from Client\n");
            pthread_exit(NULL);
        }
        char dataPath[strlen(projectName) + 62];
        memcpy(dataPath, projectName, strlen(projectName));
        memcpy(&dataPath[strlen(projectName)], "/.data/commit", 14);
        exists = access(dataPath, F_OK);
        if(exists == -1) {
            mkdir(dataPath, 0777);
        }
        int fileSize = dataSize(client_socket);
        data = retrieveData(fileSize, client_socket);
        char hash[41];
        read(client_socket, hash, 1);
        read(client_socket, hash, 40);
        hash[40] = '\0';
        memcpy(&dataPath[strlen(projectName)], "/.data/commit/.Commit", 21);
        memcpy(&dataPath[strlen(projectName) + 21], hash, 41);
        int fd = open(dataPath, O_CREAT | O_WRONLY);
        write(fd, data, strlen(data));
        write(client_socket, "done@", 5);
    }

    // If action is rollback
    else if(strcmp(actions, "rollback") == 0){
        int ver = dataSize(client_socket);
        rollback(client_socket, ver, projectName);
    }

    // If action is push
    else if(strcmp(actions, "push") == 0) {
        push(client_socket, projectName);
    }

    pthread_mutex_unlock(projMut);
    printf("Disconnected from Client\n");
    pthread_exit(NULL);
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
    sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL);

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

    // Initialize Mutexes
    createMut = malloc(sizeof(pthread_mutex_t)); 
    pthread_mutex_init(createMut, NULL);
    DIR* currDir = opendir(".");
    recursiveTraverse(currDir, "./", "mutex", NULL);
    // Accept Connections 
    while(1) {
        int client_socket;
        client_socket = accept(server_socket, NULL, NULL);  
        pthread_t t;
        int *pclient = malloc(sizeof(int));
        *pclient = client_socket;
        printf("Connected to Client!\n");
        pthread_create(&t, NULL, connection_handler, pclient);//creating the thread
    }
    close(server_socket);
        
    return 0;
}