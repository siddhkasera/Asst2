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

// Struct to store the mutexes per project
typedef struct projectMutexes {
    pthread_mutex_t * mutex;
    char* project;
    struct projectMutexes* next;
} projectMutexes;

// Globals for mutexes so threads can lock them
projectMutexes* mutexes = NULL;
projectMutexes* mutPtr = NULL;
pthread_mutex_t* createMut;

int server_socket;

// Signal Handler for exiting Server
void intHandler(int sig_num){
    close(server_socket);
    while(mutexes != NULL) {
        free(mutexes -> project);
        free(mutexes -> mutex);
        projectMutexes* temp = mutexes;
        mutexes = mutexes -> next;
        free(temp);
    }
    free(createMut);
    printf("Server Closed\n");
    exit(0);
}

// Converts integer to string
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

// Gets from client the size of the incoming data
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
char* readFromFile(char* filePath, int* filesize){
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
        if(readstatus < 100) {
            break;
        }
    }
    close(fd);
    data[bytesread] = '\0';
    if(filesize != NULL) {
        *filesize = bytesread;
    }
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
int recursiveTraverse(DIR* directory, char* dirPath, char* action, char* projectName) {
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
            // Appends to mutexes global list
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
            else {
                recursiveTraverse(dir, name, action, projectName);
            }
        } else {
            if(strcmp(action, "destroy") == 0){
                remove(name);
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
int checkout(int client_socket, char* projectName, char* action) {
    // Creates tar file without history and .data
    char tar[4*strlen(projectName) + 57];
    sprintf(tar, "tar --exclude='%s/.data' --exclude='%s/.history' -vzcf %s.tar %s", projectName, projectName, projectName, projectName);
    system(tar);

    // Sends tar file over to client
    char tarname[strlen(projectName) + 5];
    sprintf(tarname, "%s.tar", projectName);
    int* tarsize = malloc(sizeof(int));
    char* data = readFromFile(tarname, tarsize);
    char* space = intSpace(*tarsize);
    write(client_socket, "sending@", 8);
    write(client_socket, space, strlen(space));
    write(client_socket, "@", 1);
    write(client_socket, data, *tarsize);
    write(client_socket, "@", 1);
    remove(tarname);

    free(data);
    free(space);
    return 0;
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
    char dirPath[2*strlen(projectName) + 12 + strlen(verString)];
    sprintf(dirPath, "%s/.data/%s%s.tar", projectName, projectName, verString);

    // Checks if the project version exists
    int exists = access(dirPath, F_OK);
    if(exists == -1){
        write(client_socket, "ERROR@", 6);
        return;
    }

    // Makes the History Path and stores old data
    char historyPath[strlen(projectName) + 10];
    sprintf(historyPath, "%s/.history", projectName);
    char* data = readFromFile(historyPath, NULL);

    // Makes Path for Tar file
    char newPath[strlen(verString) + strlen(projectName) + 5];
    sprintf(newPath, "%s%s.tar", projectName, verString);
    rename(dirPath, newPath);

    // Destroys old project version
    DIR* currVer = opendir(projectName);
    recursiveTraverse(currVer, projectName, "destroy", projectName);

    // Creates tar command and untars the new version
    char tar[strlen(newPath) + 9];
    sprintf(tar, "tar -xf %s", newPath);
    system(tar);
    remove(dirPath);
    remove(newPath);

    // Rewrites history file
    remove(historyPath);
    int history = open(historyPath, O_CREAT | O_RDWR, 0777);
    write(history, data, strlen(data));
    write(history, "\n", 1);
    write(history, "Rollback to ", 12);
    write(history, verString, strlen(verString));
    write(history, "\n", 1);
    close(history);
    free(verString);
    write(client_socket, "complete@", 9);
}

// Push Commit Changes
int push(int client_socket, char* projectName) {
    write(client_socket, "exists@", 7);
    // Gets Commit Data from the client
    int commitSize = dataSize(client_socket);
    if(commitSize == -1) {
        return -1;
    }
    char* commitData = retrieveData(commitSize, client_socket);
    if(commitData == NULL) {
        return -1;
    }

    // Checks for Commit if it exists on the server
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
        char* data = readFromFile(dirPath, NULL);
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

    // Destroys commit directory and expires old commits
    memcpy(&dirPath[strlen(projectName) + 13], "\0", 1);
    commitDir = opendir(dirPath);
    recursiveTraverse(commitDir, dirPath, "destroy", projectName);
    
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

    // Tars Old Project Version
    char tar[strlen(projectDup) + strlen(projectName) + 16];
    sprintf(tar, "tar -vzcf %s.tar %s", projectDup, projectName);
    system(tar);
    DIR* dup = opendir(projectDup);

    // Reads in Files from the client and make necessary changes
    int numoffiles = dataSize(client_socket);
    int i;
    for(i = 0; i < numoffiles; i++) {
        // Reads in Action
        char action[2];
        read(client_socket, action, 1);
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
        // Handles modifications and addtions
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
            // Empty File Case
            if(fileSize != 0) {
                char* data = retrieveData(fileSize, client_socket);
                write(fd, data, strlen(data));
                read(client_socket, action, 1);
            }
            close(fd);
        }
        write(client_socket, "done@", 5);
    }

    // Updates Manifest with Client's manifest
    remove(manifestPath);
    int manifest = open(manifestPath, O_CREAT | O_RDWR, 0777);
    int manifestSize = dataSize(client_socket);
    char* manifestData = retrieveData(manifestSize, client_socket);
    write(client_socket, "recieved@", 9);
    write(manifest, manifestData, manifestSize);
    close(manifest);

    // Creates History path and gets New Version
    int newVer = atoi(verString) + 1;
    verString = intSpace(newVer);
    char dataPath[strlen(projectName) + 10];
    memcpy(dataPath, projectName, strlen(projectName));
    memcpy(&dataPath[strlen(projectName)], "/.history", 10);
    char* data = readFromFile(dataPath, NULL);

    // Rewrites History with new data
    int history = open(dataPath, O_CREAT | O_RDWR, 0777);
    write(history, data, strlen(data));
    write(history, "\n", 1);
    write(history, verString, strlen(verString));
    write(history, "\n", 1);
    write(history, commitData, strlen(commitData));
    close(history);

}

// Handles all connections to client
void * connection_handler(void * p_client_socket){

    int client_socket = *((int*)p_client_socket);
    free(p_client_socket);
    // Reads in Action and Project Name
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
            pthread_mutex_unlock(createMut);
            pthread_exit(NULL);
        }
        int mutExists = 0;
        projectMutexes* createPtr = mutexes;
        projectMutexes* prev = NULL;
        // Searches if mutex already exists from previous verison
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
        // Creates new mutex
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
        free(actions);
        pthread_mutex_unlock(projMut);
        pthread_mutex_unlock(createMut);
        printf("Disconnected from Client\n");
        pthread_exit(NULL);
    }

    // Locks Mutex for project Directory
    projectMutexes* ptr = mutexes;
    while(ptr != NULL) {
        if(strcmp(ptr -> project, projectName) == 0) {
            projMut = ptr -> mutex;
            pthread_mutex_lock(projMut);
            break;
        }
        ptr = ptr -> next;
    }

    // Checks if Project exists
    int exists = access(projectName, F_OK);
    if(exists == -1) {
        free(projectName);
        free(actions);
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
        free(actions);
        write(client_socket, "success@", 8);
        char* response = readAction(client_socket);
        if(strcmp(response, "recieved") != 0) {
            printf("ERROR: Message not recieved by Client\n");  
        }
        free(response);
    }

    // If action is checkout
    else if(strcmp(actions, "checkout") == 0) {
        checkout(client_socket, projectName, actions);
        free(projectName);
    }

    // If action is upgrade
    else if(strcmp(actions, "upgrade") == 0) {
        write(client_socket, "exists@", 7);
        free(actions);
        actions = readAction(client_socket);
        while(strcmp(actions, "upgrade") == 0) {
            free(actions);
            projectName = readProjectName(client_socket);
            char* data = readFromFile(projectName, NULL);
            write(client_socket, "sending@", 8);
            char* number = intSpace(strlen(data));
            write(client_socket, number, strlen(number));
            write(client_socket, "@", 1);
            if(strlen(data) != 0) {
                write(client_socket, data, strlen(data));
            }
            free(data);
            free(number);
            free(projectName);
            readAction(client_socket);
        }
        if(strcmp(actions, "request") == 0) {
            projectName = readProjectName(client_socket);

            // Make Manifest Path
            char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
            memcpy(manifestPath, projectName, strlen(projectName));
            memcpy(&manifestPath[strlen(projectName)], "/", 1);
            memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);

            char* data = readFromFile(manifestPath, NULL);
            free(manifestPath);
            write(client_socket, data, strlen(data));
            write(client_socket, "@", 1);
            free(data);
            free(actions);
            actions = readAction(client_socket);
        }
        if(strcmp(actions, "done") == 0) {
            printf("Disconnected from Client\n");
            free(projectName);
            free(actions);
            pthread_mutex_unlock(projMut);
            pthread_exit(NULL);
        }
        
    }

    // If action is history
    else if(strcmp(actions, "history") == 0) {
        char dataPath[strlen(projectName) + 10];
        memcpy(dataPath, projectName, strlen(projectName));
        memcpy(&dataPath[strlen(projectName)], "/.history", 10);
        char* data = readFromFile(dataPath, NULL);
        write(client_socket, "sending@", 8);
        char* space = intSpace(strlen(data));
        write(client_socket, space, strlen(space));
        write(client_socket, "@", 1);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
        free(data);
        free(space);
        free(projectName);
        free(actions);
        readAction(client_socket);
    }

    // If action is update or currVer
    else if(strcmp(actions, "update") == 0 || strcmp(actions, "currVer") == 0) {
        char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
        memcpy(manifestPath, projectName, strlen(projectName));
        memcpy(&manifestPath[strlen(projectName)], "/", 1);
        memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);
        char* data = readFromFile(manifestPath, NULL);
        free(manifestPath);
        free(projectName);
        write(client_socket, "sending@", 8);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
        free(data);
        free(actions);
        readAction(client_socket);
    }


    // If action is commit
    else if(strcmp(actions, "commit") == 0) {
        // Creates Manifest Path
        char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
        memcpy(manifestPath, projectName, strlen(projectName));
        memcpy(&manifestPath[strlen(projectName)], "/", 1);
        memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10);
        char* data = readFromFile(manifestPath, NULL);
        free(manifestPath);

        // Sends Manifest Data to client
        write(client_socket, "sending@", 8);
        write(client_socket, data, strlen(data));
        write(client_socket, "@", 1);
        actions = readAction(client_socket);
        if(strcmp(actions, "ERROR") == 0) {
            pthread_mutex_unlock(projMut);
            printf("Disconnected from Client\n");
            pthread_exit(NULL);
        }
        // Creates new Commit
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
        remove(dataPath);
        int fd = open(dataPath, O_CREAT | O_WRONLY, 0777);
        write(fd, data, strlen(data));
	    close(fd);
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
