#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <openssl/sha.h>

typedef struct file {
    char* status;
    char* filePath;
    char* hash;
    int fileVersion;
    struct file* next;
}file;

char* hostname;
char* port;
int network_socket = -1;
int sVer;
int cVer;
int success = 0;

void intHandler(int sig_num){
    free(hostname);
    free(port);
    printf("Connection Attempt Stopped\n");
    exit(0);
}

// Convert Integer into String
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

// Converts SHA1 Binary to Hex
void convertSHA1BinaryToCharStr(const unsigned char * const hashbin, char * const hashstr) {
    int i;
    for(i = 0; i < 20; ++i){
        sprintf(&hashstr[i*2], "%02X", hashbin[i]);
    }
    hashstr[40]=0;
}

// Clean Up after execution
void cleanUp(){
    if(network_socket >= 0) {
        close(network_socket);
    }
    free(hostname);
    free(port);
}

// Set Configurations for Port and Hostname
int setConfigure() {

    int fd = open(".configure", O_RDONLY);

    // Check if file opened properly
    if(fd == -1){
        printf("ERROR: Configure not set\n");
        return -1;
    }

    // Malloc space for Hostname/Port and do error checking
    hostname = malloc(256*sizeof(char));
    if(hostname == NULL) {
        close(fd);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }
    port = malloc(6*sizeof(char));
    if(port == NULL) {
        free(hostname);
        close(fd);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    //File IO
    char* buffer = hostname;
    int bytesread = 0;
    int readstatus = 1;

    while(readstatus > 0) {
        readstatus = read(fd, &buffer[bytesread], 1);
        if(readstatus == -1){
            free(hostname);
            free(port);
            close(fd);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        if(readstatus != 0) {
            if(buffer[bytesread] == ' '){
                buffer[bytesread] = '\0';
                bytesread = 0;
                buffer = port;
            } else {
                bytesread++;
            }
        }
    }

    // If file was Empty
    if(bytesread == 0) {
        free(port);
        free(hostname);
        close(fd);
        printf("ERROR: Configure not set\n");
        return -1;
    }
    buffer[bytesread] = '\0';
    close(fd);
    return 0;
    
}

// Connects to Server
int connectServer() {
    // Set Configurations for Port and Address
    int cf = setConfigure();
    if(cf == -1){
        return -1;
    }

    // Create a Socket
    network_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(network_socket < 0){
        cleanUp();
        printf("ERROR: In creating the socket\n");
        return -1;
    }

    // Specify an address for socket
    struct sockaddr_in server_address;
    struct hostent* results = gethostbyname(hostname);
    if(results == NULL){
        cleanUp();
        printf("ERROR: No such host\n");
        return -1;
    }

    bzero((char*)&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(port));
    bcopy( (char*)results->h_addr, (char*)&server_address.sin_addr.s_addr , results->h_length );
    

    // Connect Socket to Address
    int connection_status = connect(network_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    signal(SIGINT, intHandler);
    // Check if there was an error with connection
    while(connection_status == -1){
        printf("ERROR: Could not make connection to the remote socket\n");
        printf("Trying to connect...\n");
        sleep(3);
        connection_status = connect(network_socket, (struct sockaddr*) &server_address, sizeof(server_address));
    }
    printf("Connection to Server Made!\n");
    return 0;
}

// Read Server Response 
char* serverResponse() {
    char* actions = malloc(10*sizeof(char));
    if(actions == NULL) {
        printf("ERROR: %s\n", strerror(errno));
        return NULL;
    }
    int bytesread = 0;
    while(bytesread < 10) {
        if(read(network_socket, &actions[bytesread], 1) < 0){
            printf("ERROR: %s\n", strerror(errno));
            free(actions);
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

// Read in Data Size from Server
int dataSize(){
    char buffer[2];
    buffer[1] = '\0';
    int fileSize = 0;
    read(network_socket, buffer, 1);
    fileSize = atoi(buffer);

    while(1){
        if(read(network_socket, buffer, 1) < 0){
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
char* retrieveData(int fileSize){
    char* fileData = malloc((fileSize+1)*sizeof(char));
    if(fileData == NULL){
        printf("ERROR: %s", strerror(errno));
        return NULL;
    }
    
    int i;
    for(i = 0; i < fileSize; i++){
        if(read(network_socket, &fileData[i], 1) < 0){
            free(fileData);
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
        }
    }
    fileData[fileSize] = '\0';
    return fileData;
}

// Free Files read from the manifest
void freeFiles(file* files){
    file* ptr = files;
    while (ptr != NULL) {
        free(ptr ->filePath);
        free(ptr -> hash);
        file* temp = ptr;
        ptr = ptr -> next;
        free(temp);
    }
}

// Update Manifest
int updateManifest(char* manifest, file* files){
    remove(manifest);
    int fd = open(manifest, O_CREAT | O_WRONLY, 0777);

    if(fd == -1) {
        printf("ERROR: Could not open manifest\n");
        return -1;
    }

    char* number = intSpace(cVer);
    if(number == NULL) {
        close(fd);
        return -1;
    }
    write(fd, number, strlen(number));
    write(fd, "\n", 1);
    free(number);

    file* ptr = files;

    int i;
    while(ptr != NULL) {
        char* verString = intSpace(ptr -> fileVersion);
        if(verString == NULL) {
            close(fd);
            return -1;
        }        
        write(fd, verString, strlen(verString));
        write(fd, " ", 1);
        write(fd, ptr -> filePath, strlen(ptr -> filePath));
        write(fd, " ", 1);
        write(fd, ptr -> hash, strlen(ptr -> hash));
        write(fd, "\n", 1);
        ptr = ptr -> next;
    }
    close(fd);
    return 0;
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
        if(bytesread < 100) {
            break;
        }
    }
    data[bytesread] = '\0';
    return data;

}

// Create or Destroy a project
int createAndDestroy(char* action, char* project){
    // Write to Server the Action performed and Name of Project
    write(network_socket, action, strlen(action));
    write(network_socket, "@", sizeof(char));
    write(network_socket, project, strlen(project));
    write(network_socket, "@", sizeof(char));

    // Read Server response back
    char* actions = serverResponse(network_socket);
    if(actions == NULL) {
        return -1;
    }

    // If there was an error...
    if(strcmp(actions, "ERROR") == 0) {
        if(strcmp("create", action) == 0) {
            printf("ERROR: Project Already Exists\n");
        } else {
            printf("ERROR: Project Doesn't Exist\n");
        }
        free(actions);
        return -1;
    }

    // If the server is sending the manifest file...
    else if(strcmp(actions, "sending") == 0) {
        free(actions);
        // Read in size of data
        int fileSize = dataSize();
        if(fileSize == -1) {
            return -1;
        }

        // Read in Manifest Data
        char* fileData = retrieveData(fileSize);
        if(fileData == NULL) {
            return -1;
        }
        write(network_socket, "recieved@", 9);

        // Make the Project Directory
        mkdir(project, 0777);

        // Make the Manifest File  
        char filePath[strlen(project) + 11];
        memcpy(filePath, project, strlen(project));
        memcpy(&filePath[strlen(project)], "/", 1);
        memcpy(&filePath[strlen(project) + 1], ".manifest", 10);

        int fd = open(filePath, O_RDWR | O_CREAT, 0777);
        if(fd == -1){
            free(fileData);
            printf("ERROR: Could not open Manifest\n");
            return -1;
        }

        write(fd, fileData, fileSize);
        close(fd);
        free(fileData);
        printf("Project Successfully Created\n");
        return 0;
    }
    free(actions);
    write(network_socket, "recieved@", 9);
    printf("Project Successfully Destroyed\n");
    return 0;
}

// Create Hash for manifest
char* createHash(char* filePath) {
    int fd = open(filePath, O_RDONLY);
    int size = 100;
    char* data = malloc(101*sizeof(char));
    memset(data, 0, sizeof(data));
    int bytesread = 0;
    int readstatus = 1;
    while(1) {
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
    close(fd);

    int len;
    if(bytesread == 0){
        len = 0;
    } else {
        len = strlen(data);
    }
    char* hash = malloc(SHA_DIGEST_LENGTH*sizeof(char));
    SHA1(data, len, hash);
    hash[SHA_DIGEST_LENGTH] = '\0';
    char* properHash = malloc(41*sizeof(char));
    convertSHA1BinaryToCharStr(hash, properHash);
    return properHash;
}

// Read from Manifest
file* readManifest(int fd, char* actions, char* fileName, int client){
    int found = -1;
    if(strcmp(actions, "add") == 0 || strcmp(actions, "remove") == 0) {
        found = 0;
    }
    int num = 0;
    file* files = NULL;
    file* ptr = files;
    int readstatus = 1;
    char buffer[2];

    // Read in Manifest Version Number
    int ver = 0;
    buffer[1] = '\0';
    
    readstatus = read(fd, buffer, 1);
    if(readstatus == -1){
        printf("ERROR: %s\n", strerror(errno));
        return NULL;
    }

    ver = atoi(buffer);
    while(1) {
        readstatus = read(fd, buffer, 1);
        if(readstatus == -1){
            printf("ERROR: %s\n", strerror(errno));
            return NULL;
        }
        if(buffer[0] == '\n') {
            break;
        }
        ver *= 10;
        ver += atoi(buffer);
    }

    if(client) {
        cVer = ver;
    } else {
        sVer = ver;
    }

    while(1) {
        // Read Status
        readstatus = read(fd, &buffer, 1);
        if(readstatus == 0 || buffer[0] == '@'){
            break;
        }
        if(readstatus == -1) {
            printf("ERROR: %s\n", strerror(errno));
            freeFiles(files);
            return NULL;
        }

        // Create an entry
        file* newFile = malloc(sizeof(file));
        if(newFile == NULL) {
            printf("ERROR: %s\n", strerror(errno));
            freeFiles(files);
            return NULL;
        }
        
        // Read in File Version
        newFile -> fileVersion = atoi(buffer);

        while(1) {
            read(fd, &buffer, 1);
            if(readstatus == -1) {
                printf("ERROR: %s\n", strerror(errno));
                free(newFile);
                freeFiles(files);
                return NULL;
            }
            if(buffer[0] == ' ') {
                break;
            }
            newFile -> fileVersion *= 10;
            newFile -> fileVersion += atoi(buffer);
        }

        // Read in filepath
        int bytesread = 0;
        int size = 100;
        char* filePath = malloc(size*sizeof(char));
        while(1) {
            if(read(fd, &filePath[bytesread],  1) < 0) {
                ptr -> next = NULL;
                free(newFile);
                free(filePath);
                freeFiles(files);
                printf("ERROR: %s\n", strerror(errno));
                return NULL;
            }
            if(filePath[bytesread] == ' '){
                filePath[bytesread] = '\0';
                break;
            }
            if(bytesread >= size -2) {
                char* temp = filePath;
                size *= 2;
                filePath = malloc(size*sizeof(char));
                if(filePath == NULL) {
                    free(newFile);
                    freeFiles(files);
                    free(temp);
                    printf("ERROR: %s\n", strerror(errno));
                    return NULL;
                }
                memcpy(filePath, temp, bytesread + 1);
                free(temp);
            }
            bytesread++;
        }

        newFile -> filePath = filePath;

        // If Current File matches Request File
        if(found != -1 && strcmp(filePath, fileName) == 0){
            found = 1;
            free(newFile -> filePath);
            free(newFile);
            if(strcmp(actions, "add") == 0){
                printf("Warning: File already exists in Manifest\n");
                freeFiles(files);
                return NULL;
            } else {
                char* useless[41];
                read(fd, useless, 41);
                continue;
            }
        } else {
            // Read in Hash
            char* hash = malloc(41*sizeof(char));
            readstatus = read(fd, hash, 41);
            if(readstatus == -1){
                free(hash);
                free(newFile -> filePath);
                free(newFile);
                freeFiles(files);
                printf("ERROR: %s\n", strerror(errno));
                return NULL;
            }
            hash[40] = '\0';
            newFile -> hash = hash;
        }

        if(num == 0) {
            files = newFile;
            ptr = files;
            files -> next = NULL;
        } else {
            ptr -> next = newFile;
            ptr = ptr -> next;
            ptr -> next = NULL;
        }
        num++;
        
    }

    // If File was not found in the manifest
    if(found == 0){
        if(strcmp(actions, "remove") == 0) {
            printf("Warning: No such file exists in .manifest\n");
            freeFiles(files);
            return NULL;
        }
        if(files == NULL) {
            files = malloc(sizeof(file));
            files -> filePath = fileName;
            files -> hash = createHash(fileName);
            files -> fileVersion = 0;
        } else {
            ptr -> next = malloc(sizeof(file));
            ptr -> next -> filePath = fileName;
            ptr -> next -> hash = createHash(fileName);
            ptr -> next -> fileVersion = 0;
        }
        
    }
    success = 1;
    return files;
}

// Produces a .Update file for users on client side
int update(char* projName, char * updatePath, char * manifestPath){
    write(network_socket, "update@", 7);
    write(network_socket, projName, strlen(projName));
    write(network_socket, "@", 1);

    char* actions = serverResponse();
    if(strcmp(actions, "ERROR") == 0) {
        printf("ERROR: Project Did not exist on server\n");
        return -1;
    }

    file* serverManifest = readManifest(network_socket, "update", NULL, 0);

    int manifestFile = open(manifestPath, O_RDONLY);
    file* clientManifest = readManifest(manifestFile, "update", NULL, 1);
    close(manifestFile);
    remove(updatePath);
    int updateFile = open(updatePath, O_CREAT | O_WRONLY);

    if(cVer == sVer) {
        printf("All up to date!\n");
        close(updateFile);
        freeFiles(clientManifest);
        freeFiles(serverManifest);
        return 0;
    }

    cVer = sVer;

    int conflictFile = -1;
    char* conflictPath = malloc((strlen(projName) + 11)*sizeof(char));
    memcpy(conflictPath, projName, strlen(projName));
    memcpy(&conflictPath[strlen(projName)], "/", 1);
    memcpy(&conflictPath[strlen(projName) + 1], ".Conflict", 10);
    remove(conflictPath);

    file* serverPtr = serverManifest;

    while(serverPtr != NULL) {
        file* prev = NULL;
        file* clientPtr = clientManifest;
        while(clientPtr != NULL) {
            if(strcmp(clientPtr -> filePath, serverPtr -> filePath) == 0) {
                break;
            }
            prev = clientPtr;
            clientPtr = clientPtr -> next;
        }
        if(clientPtr == NULL) {
            write(updateFile, "A ", 2);
            write(updateFile, serverPtr -> filePath, strlen(serverPtr -> filePath));
            write(updateFile, " ", 1);
            write(updateFile, serverPtr -> hash, 40);
            write(updateFile, "\n", 1);
            printf("A %s\n", serverPtr -> filePath);
            serverPtr = serverPtr -> next;
            continue;
        } else if(strcmp(clientPtr -> hash, serverPtr -> hash) != 0) {
            char* liveHash = createHash(clientPtr -> filePath);
            if(strcmp(liveHash, clientPtr -> hash) != 0) {
                if(conflictFile == -1) {
                    conflictFile = open(conflictPath, O_CREAT | O_RDWR, 0777);
                }
                write(conflictFile, "C ", 2);
                write(conflictFile, serverPtr -> filePath, strlen(serverPtr -> filePath));
                write(conflictFile, " ", 1);
                write(conflictFile, liveHash, 40);
                write(conflictFile, "\n", 1);
                printf("C %s\n", serverPtr -> filePath);
            } else {
                write(updateFile, "M ", 2);
                write(updateFile, serverPtr -> filePath, strlen(serverPtr -> filePath));
                write(updateFile, " ", 1);
                write(updateFile, serverPtr -> hash, 40);
                write(updateFile, "\n", 1);
                printf("M %s\n", serverPtr -> filePath);
            }
        }
        free(clientPtr -> hash);
        free(clientPtr -> filePath);
        if(prev == NULL) {
            clientManifest = clientPtr -> next;
            free(clientPtr);
        } else {
            file* temp = clientPtr;
            prev -> next = clientPtr -> next;
            free(temp);
        }
        serverPtr = serverPtr -> next;
    }
    file* ptr = clientManifest;
    while(ptr != NULL){
        write(updateFile, "D ", 2);
        write(updateFile, ptr -> filePath, strlen(ptr -> filePath));
        write(updateFile, " ", 1);
        write(updateFile,  ptr -> hash, 40);
        write(updateFile, "\n", 1);
        printf("D %s\n", ptr -> filePath);
        ptr = ptr -> next;
    }
    freeFiles(clientManifest);
    freeFiles(serverManifest);
    if(conflictFile != -1) {
        remove(updatePath);
    }
    return 0;
}

// Gets Current Version and status of a project
int currVer(char* projectName){
    write(network_socket, "currVer@", 8);
    write(network_socket, projectName, strlen(projectName));
    write(network_socket, "@", sizeof(char));

    file* serverManifest = readManifest(network_socket, "currVer", NULL, 0);
    printf("%d\n", sVer);

    file* ptr = serverManifest;
    while(ptr != NULL) {
        printf("%s %d\n", ptr -> filePath, ptr -> fileVersion);
        ptr = ptr -> next;
    }
    freeFiles(serverManifest);
    return 0;
}

// Create a Commit file 
int commit(char* projName, char* commitPath, char* manifestPath) {
    write(network_socket, "commit@", 7);
    write(network_socket, projName, strlen(projName));
    write(network_socket,"@",1);

    char* actions = serverResponse();
    if(actions == NULL) {
        return -1;
    }
    if(strcmp(actions, "ERROR") == 0) {
        printf("ERROR: File Did not exist on server\n");
        return -1;
    }
    free(actions);

    file* serverManifest = readManifest(network_socket, "commit", NULL, 0);
    if(serverManifest == NULL && success == 0) {
        return -1;
    }

    int manifestFile = open(manifestPath, O_RDONLY);
    if(manifestFile == -1) {
        freeFiles(serverManifest);
        printf("ERROR: Could not open .manifest\n");
        return -1;
    }

    file* clientManifest = readManifest(manifestFile, "commit", NULL, 1);
    close(manifestFile);

    if(clientManifest == NULL && success == 0) {
        freeFiles(serverManifest);
        return -1;
    }

    if(cVer != sVer) {
        printf("Client must call Update for latest files\n");
        write(network_socket, "ERROR@", 6);
        freeFiles(clientManifest);
        freeFiles(serverManifest);
        return 0;
    }

    remove(commitPath);
    int commitFile = open(commitPath, O_CREAT | O_WRONLY);
    if(commitFile == -1) {
        printf("ERROR: Could not open .Commit\n");
        freeFiles(clientManifest);
        freeFiles(serverManifest);        
        return -1;
    }

    file* clientPtr = clientManifest;
    int changes = 0;
    while(clientPtr != NULL) {
        file* prev = NULL;
        file* serverPtr = serverManifest;
        while(serverPtr != NULL) {
            if(strcmp(clientPtr -> filePath, serverPtr -> filePath) == 0) {
                break;
            }
            prev = serverPtr;
            serverPtr = serverPtr -> next;
        }
        char* liveHash = createHash(clientPtr -> filePath);
        if(serverPtr == NULL) {
            write(commitFile, "A ", 2);
            write(commitFile, clientPtr -> filePath, strlen(clientPtr -> filePath));
            write(commitFile, " ", 1);
            write(commitFile, liveHash, 40);
            write(commitFile, "\n", 1);
            printf("A %s\n", clientPtr -> filePath);
            clientPtr = clientPtr -> next;
            changes = 1;
            continue;
        } else if(strcmp(clientPtr -> hash, serverPtr -> hash) != 0) {
            freeFiles(clientManifest);
            freeFiles(serverManifest);
            close(commitFile);
            remove(commitPath);
            printf("ERROR: Client must synch with repo, client hash did not match server\n");
            return -1;
        } else if(strcmp(clientPtr -> hash, liveHash) != 0) {
            write(commitFile, "M ", 2);
            write(commitFile, clientPtr -> filePath, strlen(clientPtr -> filePath));
            write(commitFile, " ", 1);
            write(commitFile, liveHash, 40);
            write(commitFile, "\n", 1);
            printf("M %s\n", clientPtr -> filePath);
            changes = 1;
        }
        free(serverPtr -> hash);
        free(serverPtr -> filePath);
        if(prev == NULL) {
            serverManifest = serverPtr -> next;
            free(serverPtr);
        } else {
            file* temp = serverPtr;
            prev -> next = serverPtr -> next;
            free(temp);
        }
        clientPtr = clientPtr -> next;
    }
    file* ptr = serverManifest;
    while(ptr != NULL){
        write(commitFile, "D ", 2);
        write(commitFile, ptr -> filePath, strlen(ptr -> filePath));
        write(commitFile, " ", 1);
        write(commitFile,  ptr -> hash, 40);
        write(commitFile, "\n", 1);
        printf("D %s\n", ptr -> filePath);
        ptr = ptr -> next;
        changes = 1;
    }
    freeFiles(clientManifest);
    freeFiles(serverManifest);
    close(commitFile);
    if(changes == 0) {
        printf("No changes to commit\n");
        write(network_socket, "ERROR@", 6);
        remove(commitPath);
        return 0;
    }
    char* data = readFromFile(commitPath);
    if(data == NULL) {
        return -1;
    }
    char* space = intSpace(strlen(data));
    if(space == NULL) {
        free(data);
        return -1;
    }
    write(network_socket, "sending@", 8);
    write(network_socket, space, strlen(space));
    write(network_socket, "@", 1);
    write(network_socket, data, strlen(data));
    write(network_socket, "@", 1);
    free(data);
    free(space);
    char* hash = createHash(commitPath);
    if(hash == NULL) {
        return -1;
    }
    write(network_socket, hash, 40);
    write(network_socket, "@", 1);
    free(hash);
    actions = serverResponse();
    if(strcmp(actions, "done") == 0) {
        printf("Commit Successful!\n");
    }
    free(actions);
    return 0;    

}

// Upgrade files in client side 
int upgrade(char* manifestPath, char* updatePath, char* projectName) {
    write(network_socket, "upgrade@", 8);
    write(network_socket, projectName, strlen(projectName));
    write(network_socket, "@", 1);

    char* response = serverResponse();
    if(strcmp(response, "ERROR") == 0) {
        printf("ERROR: Project does not exist on Server\n");
        return -1;
    }
    free(response);

    int empty = 1;
    int updateFile = open(updatePath, O_RDONLY);
    if(updateFile == -1) {
        printf("ERROR: Could not open .Update file\n");
    }

    int readstatus = 1;
    while(readstatus > 0) {
        char action[2];
        readstatus = read(updateFile, action, 2);
        if(readstatus == 0){
            break;
        }
        empty = 0;
        action[1] = '\0';

        // Read in filepath
        int bytesread = 0;
        int size = 100;
        char* filePath = malloc(size*sizeof(char));
        while(1) {
            if(read(updateFile, &filePath[bytesread], 1) < 0) {
                free(filePath);
                close(updateFile);
                printf("ERROR: %s\n", strerror(errno));
                return -1;
            }
            if(filePath[bytesread] == ' '){
                filePath[bytesread] = '\0';
                break;
            }
            if(bytesread >= size -2) {
                char* temp = filePath;
                size *= 2;
                filePath = malloc(size*sizeof(char));
                if(filePath == NULL) {
                    close(updateFile);
                    free(temp);
                    printf("ERROR: %s\n", strerror(errno));
                    return -1;
                }
                memcpy(filePath, temp, bytesread + 1);
                free(temp);
            }
            bytesread++;
        }

        // Read in Hash
        char* hash = malloc(41*sizeof(char));
        readstatus = read(updateFile, hash, 41);
        if(readstatus == -1){
            free(hash);
            free(filePath);
            close(updateFile);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        hash[40] = '\0';

        if(action[0] == 'M' || action[0] == 'A') {
            write(network_socket, "upgrade@", 8);
            write(network_socket, filePath, strlen(filePath));
            write(network_socket, "@", 1);

            int bytesread = 0;
            
            // Read Server response back
            char* actions = serverResponse();
            if(actions == NULL) {
                free(filePath);
                free(hash);
                return -1;
            }

            if(strcmp(actions, "sending") == 0) {   
                free(actions);  

                // Read in size of data
                int fileSize = dataSize();
                if(fileSize == -1) {
                    free(filePath);
                    free(hash);                  
                    return -1;
                }

                // Read in Data from Server
                char* fileData = retrieveData(fileSize);
                if(fileData == NULL) {
                    free(filePath);
                    free(hash);                      
                    return -1;
                }
                remove(filePath);
                int file = open(filePath, O_CREAT | O_RDWR, 0777);
                write(file, fileData, fileSize);
                close(file);
            }
        }
    }
    remove(updatePath);
    if(empty) {
        write(network_socket, "done@", 5);
        printf("Project is Up to Date!\n");
        return 0;
    }

    write(network_socket, "request@", 8);
    write(network_socket, projectName, strlen(projectName));
    write(network_socket, "@", 1);
    file* files = readManifest(network_socket, "upgrade", NULL, 0);
    write(network_socket, "done@", 5);
    cVer = sVer;
    int status = updateManifest(manifestPath, files);
    freeFiles(files);
    if(status == 0) {
        printf("Upgrade Successful!\n");
    }
    return status;
}

// Checkout a Project from the server
int checkout(char* projName) {
    write(network_socket, "checkout@", 9);
    write(network_socket, projName, strlen(projName));
    write(network_socket, "@", 1);

    mkdir(projName, 0777);
    int bytesread = 0;

    // Read Server response back
    char* actions = serverResponse();
    if(actions == NULL) {
        return -1;
    }

    if(strcmp(actions, "ERROR") == 0) {
        printf("ERROR: Project Did not exist on server\n");
        return -1;
    }
    free(actions);

    // Read From Server Number of Directories
    int numOfDir = dataSize();
    if(numOfDir == -1) {
        return -1;
    }
    char buffer[2];
    buffer[1] = '\0';
    

    int i;
    for(i = 0; i < numOfDir; i++) {
        int size = 100;
        char* dirName = malloc(100*sizeof(char));
        if(dirName == NULL) {
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        bytesread = 0;
        while(1) {
            read(network_socket, &dirName[bytesread], 1);
            if(dirName[bytesread] == '@') {
                dirName[bytesread] = '\0';
                break;
            }
            bytesread++;
            if(bytesread >= size-2) {
                size*=2;
                char* temp = dirName;
                dirName = malloc(size*sizeof(char));
                if(dirName == NULL) {
                    free(temp);
                    printf("ERROR: %s\n", strerror(errno));
                    return -1;
                }                
                memcpy(dirName, temp, bytesread);
                free(temp);
            }
        }
        mkdir(dirName, 0777);
        free(dirName);
        write(network_socket, "recieved@", 9);
    }

    int numOfFiles = dataSize();
    if(numOfFiles == -1) {
        return -1;
    }

    for(i = 0; i < numOfFiles; i++) {
        int size = 100;
        char* fileName = malloc(100*sizeof(char));
        if(fileName == NULL) {
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        bytesread = 0;
        while(1) {
            read(network_socket, &fileName[bytesread], 1);
            if(fileName[bytesread] == '@') {
                fileName[bytesread] = '\0';
                break;
            }
            bytesread++;
            if(bytesread >= size-2) {
                size*=2;
                char* temp = fileName;
                fileName = malloc(size*sizeof(char));
                if(fileName == NULL) {
                    free(temp);
                    printf("ERROR: %s\n", strerror(errno));
                    return -1;
                }
                memcpy(fileName, temp, bytesread);
                free(temp);
            }
        }
        int fd = open(fileName, O_CREAT | O_RDWR);
        if(fd == -1) {
            printf("ERROR: Could not create %s\n", fileName);
            free(fileName);
            return -1;
        }
        chmod(fileName, 0777);
        free(fileName);

        int fileSize = dataSize();
        if(fileSize == -1){
            close(fd);
            return -1;
        }
    
        char* fileData = retrieveData(fileSize);
        if(fileData == NULL) {
            close(fd);
            return -1;
        }

        write(fd, fileData, fileSize);
        read(network_socket, buffer, 1);
        write(network_socket, "recieved@", 9);
        free(fileData);
        close(fd);

    }

    return 0;

}

// Get the history of a Project
int history(char* projectName) {
    write(network_socket, "history@", 8);
    write(network_socket, projectName, strlen(projectName));
    write(network_socket, "@", sizeof(char));

    // Read Server Response
    char* actions = serverResponse();

    // Read in size of data
    int fileSize = dataSize();
    if(fileSize == -1) {
        return -1;
    }

    // Read in Manifest Data
    char* fileData = retrieveData(fileSize);
    if(fileData == NULL) {
        return -1;
    }
    fileData[fileSize] = '\0';

    printf("%s\n", fileData);
    return 0;
}

// Add or Remove from the manifest file
int addOrRemove(char* argv[], char* fileName) {

    // Read through Manifest file
    char* manifest = malloc(strlen(argv[2]) + 11);
    if(manifest == NULL){
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    memcpy(manifest, argv[2], strlen(argv[2]));
    memcpy(&manifest[strlen(argv[2])] , "/", 1 );
    memcpy(&manifest[strlen(argv[2]) + 1] , ".manifest", 10 );
    
    // Open Manifest File
    int fd = open(manifest, O_RDONLY);
    if(fd == -1) {
        free(manifest);
        printf("ERROR: Could not open .manifest file\n");
        return -1;
    }

    file* files = readManifest(fd, argv[1], fileName, 1);
    close(fd);

    if(files == NULL && success == 0) {
        free(manifest);
        return -1;
    }

    printf("Manifest Updated!\n");
    int status = updateManifest(manifest, files);
    free(manifest);
    freeFiles(files);
    return status;
}

// Pushes Commit for a Project
int push(char* projectName, char* commitPath) {
    // Writes to Server the Action and Project
    write(network_socket, "push@", 5);
    write(network_socket, projectName, strlen(projectName));
    write(network_socket, "@", 1);

    // Read in Server Response
    char* response = serverResponse();
    if(response == NULL) {
        return -1;
    }
    if(strcmp(response, "ERROR") == 0) {
        printf("ERROR: Project does not exist on the server\n");
        return -1;
    }
    free(response);
    char* commitData = readFromFile(commitPath);
    if(commitData == NULL) {
        return -1;
    }
    char* fileSize = intSpace(strlen(commitData));
    if(fileSize == NULL) {
        return -1;
    }

    // Writes to Server the Commit File Data
    write(network_socket, fileSize, strlen(fileSize));
    write(network_socket, "@", 1);
    write(network_socket, commitData, strlen(commitData));
    write(network_socket, "@", 1);
    free(commitData);
    free(fileSize);

    // Read in Server Response
    response = serverResponse();
    if(response == NULL) {
        return -1;
    }
    if(strcmp(response, "ERROR") == 0) {
        printf("ERROR: Commit not found on server\n");
        return -1;
    }

    // Manifest Path
    char* manifestPath = malloc((strlen(projectName) + 11)*sizeof(char));
    memcpy(manifestPath, projectName, strlen(projectName));
    memcpy(&manifestPath[strlen(projectName)], "/", 1);
    memcpy(&manifestPath[strlen(projectName) + 1], ".manifest", 10); 
    int manifest = open(manifestPath, O_RDONLY);
    if(manifest == -1) {
        printf("ERROR: Could not open .manifest file\n");
        return -1;
    }

    file* manifestFiles = readManifest(manifest, "push", NULL, 1); 
    cVer++;
    close(manifest);  

    if(manifestFiles == NULL) {
        free(manifestPath);
        return -1;
    }

    int fd = open(commitPath, O_RDONLY);
    if(fd == -1) {
        freeFiles(manifestFiles);
        free(manifestPath);
        return -1;
    }

    file* files = NULL;
    file* filePtr = files;
    int i;
    int readstatus = 1;
    int num = 0;

    // Read in Data from Commit File
    while(readstatus > 0) {
        char* status = malloc(2*sizeof(char));
        readstatus = read(fd, status, 2);
        if(readstatus == 0){
            break;
        }
        status[1] = '\0';

        // Read in filepath
        int bytesread = 0;
        int size = 100;
        char* filePath = malloc(size*sizeof(char));
        while(1) {
            if(read(fd, &filePath[bytesread], 1) < 0) {
                free(filePath);
                free(status);
                free(manifestPath);
                freeFiles(files);
                close(fd);
                printf("ERROR: %s\n", strerror(errno));
                return -1;
            }
            if(filePath[bytesread] == ' '){
                filePath[bytesread] = '\0';
                break;
            }
            if(bytesread >= size -2) {
                char* temp = filePath;
                size *= 2;
                filePath = malloc(size*sizeof(char));
                if(filePath == NULL) {
                    close(fd);
                    free(status);
                    freeFiles(files);
                    free(temp);
                    free(manifestPath);
                    printf("ERROR: %s\n", strerror(errno));
                    return -1;
                }
                memcpy(filePath, temp, bytesread + 1);
                free(temp);
            }
            bytesread++;
        }

        // Read in Hash
        char* hash = malloc(41*sizeof(char));
        readstatus = read(fd, hash, 41);
        if(readstatus == -1){
            free(hash);
            free(filePath);
            free(status);
            free(manifestPath);
            freeFiles(files);
            close(fd);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        hash[40] = '\0';
        
        file* newFile = malloc(sizeof(file));
        newFile -> hash = hash;
        newFile -> filePath = filePath;
        newFile -> status = status;
        newFile -> next = NULL;

        if(files == NULL) {
            files = newFile;
            filePtr = files;
        } else {
            filePtr -> next = newFile;
            filePtr = filePtr -> next;
        }
        num++;
    }

    close(fd);
    char* numString = intSpace(num);
    write(network_socket, numString, strlen(numString));
    write(network_socket, "@", 1);

    filePtr = files;
    while(filePtr != NULL) {
        write(network_socket, filePtr -> status, 1);
        write(network_socket, "@", 1);
        write(network_socket, filePtr -> filePath, strlen(filePtr -> filePath));
        write(network_socket, "@", 1);
        if(strcmp(filePtr -> status, "D") != 0) {
            file* manifestPtr = manifestFiles;
            while(strcmp(filePtr -> status, "M") == 0 && manifestPtr != NULL ) {
                if(strcmp(manifestPtr -> filePath, filePtr -> filePath) == 0) {
                    manifestPtr -> fileVersion = manifestPtr -> fileVersion + 1;
                    break;
                }
                manifestPtr = manifestPtr -> next;
            }
            char* data = readFromFile(filePtr -> filePath);
            char* size = intSpace(strlen(data));
            write(network_socket, size, strlen(size));
            write(network_socket, "@", 1);
            if(strlen(data) != 0) {
                write(network_socket, data, strlen(data));
                write(network_socket, "@", 1);
            }
        }
        filePtr = filePtr -> next;
    }
    updateManifest(manifestPath, manifestFiles);

    // Write updated Manifest to Server
    char* manifestData = readFromFile(manifestPath);
    char* manifestSize = intSpace(strlen(manifestData));
    
    write(network_socket, manifestSize, strlen(manifestSize));
    write(network_socket, "@", 1);
    write(network_socket, manifestData, strlen(manifestData));
    write(network_socket, "@", 1);

    free(manifestPath);
    freeFiles(manifestFiles);
    remove(commitPath);
    freeFiles(files);
    return 0;

}

int main(int argc, char* argv[]) { 

    // Check for Minimum Number of Arguments
    if(argc < 3) {
        printf("ERROR: Expected at least 2 arguments: <action> <project> ...\n");
        return -1;
    }

    // If user wants to set a new configuration...
    if(strcmp(argv[1], "configure") == 0) {

        // Check for Correct Number of Arguments
        if(argc != 4){
            printf("ERROR: Incorrect Format, expected: ./WTF configure <IP/hostname> <port>\n");
            return -1;
        }
        
        // Check for Proper Length and Input
        if(strlen(argv[2]) >= 255){
            printf("ERROR: Hostname can't be greater than 255 characters\n");
            return -1;
        }

        if(atoi(argv[3]) > 65000 || atoi(argv[3]) < 5000) {
            printf("ERROR: Port must be a number between 5000 - 65000\n");
            return -1;
        }

        // Create/Rewrite configure file
        remove(".configure");
        int fd = open(".configure", O_CREAT | O_RDWR, 0777);
       

        // Check if file opened properly
        if(fd == -1){
            printf("ERROR: Could not open .configure\n");
            return -1;
        }

        // Write new configurations to the file
        write(fd, argv[2], strlen(argv[2]));
        char space = ' ';
        write(fd, &space, 1);
        write(fd, argv[3], strlen(argv[3]));

        close(fd);
        return 0;
    }
    
    // If user wants to add or remove a file from manifest...
    if(strcmp(argv[1], "add") == 0 || strcmp(argv[1], "remove") == 0) {
        // Check for Correct Number of Arguments
        if(argc != 4){
            printf("ERROR: Incorrect Format, expected: ./WTF <add/remove> <projectName> <filename>\n");
            return -1;
        }

        //Check if Project exists
        int exists = access(argv[2], F_OK);
        if(exists == -1) {
            printf("ERROR: Project does not exist\n");
            return -1;
        }

        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }

        char* file = argv[3];
        if(argv[3][0] == '.' && argv[3][1] == '/') {
            file = &argv[3][2];
        }

        char* fileName = malloc((strlen(projName) + strlen(file + 2))*sizeof(char));
        if(fileName == NULL) {
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }

        memcpy(fileName, projName, strlen(projName));
        memcpy(&fileName[strlen(projName)], "/", 1);
        memcpy(&fileName[strlen(projName) + 1], file, strlen(file));

        //Check if filename exists
        exists = access(fileName, F_OK);
        if(exists == -1 && strcmp(argv[1], "add") == 0) {
            printf("ERROR: File does not exist\n");
            free(fileName);
            return -1;
        }

        int status = addOrRemove(argv, fileName);
        cleanUp();
        free(fileName);
        return status;
    }
    
    // If user wants to create or destroy a project ...
    if(strcmp("create", argv[1]) == 0 || strcmp("destroy", argv[1]) == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: <create/destroy> <projectname>\n");
            return -1;
        }

        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }

        if(connectServer() == -1) {
            return -1;
        }
        
        int status = createAndDestroy(argv[1], projName);
        cleanUp();
        return status;
    }
    
    // If user wants to checkout a project...
    if(strcmp("checkout", argv[1]) == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: checkout <projectname>\n");
            return -1;
        }
        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }
        
        if(access(argv[2], F_OK) != -1) {
            printf("ERROR: Project already exists on client\n");
            cleanUp();
            return -1;
        }

        if(connectServer() == -1) {
            return -1;
        }

        int status = checkout(projName);
        cleanUp();
        return status;
    }

    //If user wants to update a project...
    if(strcmp("update", argv[1]) == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: update <projectname>\n");
            return -1;
        }

        if(connectServer() == -1) {
            return -1;
        }

        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }

        if(access(argv[2], F_OK) == -1) {
            printf("ERROR: Project does not exist on client\n");
            cleanUp();
            return -1;
        }

        // Make Update Path
        char* updatePath = malloc((strlen(projName) + 9)*sizeof(char));
        memcpy(updatePath, projName, strlen(projName));
        memcpy(&updatePath[strlen(projName)], "/", 1);
        memcpy(&updatePath[strlen(projName) + 1], ".Update", 8);

        // Make Manifest Path
        char* manifestPath = malloc((strlen(projName) + 11)*sizeof(char));
        memcpy(manifestPath, projName, strlen(projName));
        memcpy(&manifestPath[strlen(projName)], "/", 1);
        memcpy(&manifestPath[strlen(projName) + 1], ".manifest", 10);

        int status = update(projName, updatePath, manifestPath);
        cleanUp();
        free(updatePath);
        free(manifestPath);
        return status;
    }    

    // If user wants to upgrade a project...
    if(strcmp("upgrade", argv[1]) == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: upgrade <projectname>\n");
            return -1;
        }

        if(access(argv[2], F_OK) == -1) {
            printf("ERROR: Project does not exist on client\n");
            cleanUp();
            return -1;
        }

        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }

        char* updatePath = malloc((strlen(projName) + 11)*sizeof(char));
        memcpy(updatePath, projName, strlen(projName));
        memcpy(&updatePath[strlen(projName)], "/", 1);
        memcpy(&updatePath[strlen(projName) + 1], ".Conflict", 10);

        if(access(updatePath, F_OK) != -1) {
            free(updatePath);
            cleanUp();
            printf("ERROR: Resolve all conflicts before update\n");
            return -1;
        }

        memcpy(&updatePath[strlen(projName) + 1], ".Update", 7);
        updatePath[strlen(projName) + 8]  = '\0';

        if(access(updatePath, F_OK) == -1) {
            free(updatePath);
            cleanUp();
            printf("ERROR: Call update before calling upgrade\n");
            return -1;
        }

        if(connectServer() == -1) {
            return -1;
        }
        
        char* manifestPath = malloc((strlen(projName) + 11)*sizeof(char));
        memcpy(manifestPath, projName, strlen(projName));
        memcpy(&manifestPath[strlen(projName)], "/", 1);
        memcpy(&manifestPath[strlen(projName) + 1], ".manifest", 10);

        int status = upgrade(manifestPath, updatePath, projName);
        cleanUp();
        free(manifestPath);
        free(updatePath);
        return status;
    }
    
    // If user wants to commit to a project...
    if(strcmp(argv[1], "commit") == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: commit <projectname>\n");
            return -1;
        }

        if(access(argv[2], F_OK) == -1) {
            printf("ERROR: Project does not exist on client\n");
            return -1;
        }

        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }

        char* updatePath = malloc((strlen(projName) + 11)*sizeof(char));
        memcpy(updatePath, projName, strlen(projName));
        memcpy(&updatePath[strlen(projName)], "/", 1);
        memcpy(&updatePath[strlen(projName) + 1], ".Conflict", 10);

        if(access(updatePath, F_OK) != -1) {
            printf("ERROR: Resolve all conflicts and call Update before Commit\n");
            return -1;
        }

        memcpy(&updatePath[strlen(projName) + 1], ".Update", 7);
        updatePath[strlen(projName) + 8]  = '\0';

        int fd = open(updatePath, O_RDONLY);
        if(fd != -1) {
            char buffer[1];
            if(read(fd, buffer, 1) > 0) {
                free(updatePath);
                printf("ERROR: Update before Commit\n");
                return -1;
            }
            close(fd);
        }

        char* commitPath = malloc((strlen(projName) + 9)*sizeof(char));
        memcpy(commitPath, projName, strlen(projName));
        memcpy(&commitPath[strlen(projName)], "/", 1);
        memcpy(&commitPath[strlen(projName) + 1], ".Commit", 8);

        // Make Manifest Path
        char* manifestPath = malloc((strlen(projName) + 11)*sizeof(char));
        memcpy(manifestPath, projName, strlen(projName));
        memcpy(&manifestPath[strlen(projName)], "/", 1);
        memcpy(&manifestPath[strlen(projName) + 1], ".manifest", 10);

        if(connectServer() == -1) {
            return -1;
        }  

        int status = commit(projName, commitPath, manifestPath);  
        cleanUp();
        free(commitPath);
        return status;    
    }
    
    // If user wants the history of the project...
    if(strcmp(argv[1], "history") == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: history <projectname>\n");
            return -1;
        }        
        if(connectServer() == -1) {
            return -1;
        }
        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }
        int status = history(projName);
        cleanUp();
        return status;
    }

    // If user wants the current version of the project...
    if(strcmp(argv[1], "currentversion") == 0) {
        if(argc != 3) {
            printf("ERROR: Incorrect Input, expected: currentversion <projectname>\n");
            return -1;
        }
        if(connectServer() == -1) {
            return -1;
        }
        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }
        int status = currVer(projName);
        cleanUp();
        return status;
    }

    // If user wants to rollback the project...
    if(strcmp(argv[1], "rollback") == 0) {
        if(argc != 4) {
            printf("ERROR: Incorrect Input, expected: rollback <projectname> <ver>\n");
            return -1;
        }
        if(connectServer() == -1) {
            return -1;
        }
        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }
        write(network_socket, "rollback@", 9);
        write(network_socket, projName, strlen(projName));
        write(network_socket, "@", 1);
        write(network_socket, argv[3], strlen(argv[3]));
        write(network_socket, "@", 1);
        char* response = serverResponse();
        if(strcmp(response, "ERROR") == 0) {
            printf("ERROR: Rollback Failed, Project or Version did not exist\n");
        } else {
            printf("Rollback Successful!\n");
        }
        free(response);
        cleanUp();
        return 0;
    }

    // If user wants to push a Commit...
    if(strcmp(argv[1], "push") == 0) {
        if(connectServer() == -1) {
            return -1;
        }
        char* projName = argv[2];
        if(argv[2][0] == '.' && argv[2][1] == '/') {
            projName = &argv[2][2];
        }
        // Makes the path for .Commit
        char* commitPath = malloc((strlen(projName) + 9)*sizeof(char));
        memcpy(commitPath, projName, strlen(projName));
        memcpy(&commitPath[strlen(projName)], "/", 1);
        memcpy(&commitPath[strlen(projName) + 1], ".Commit", 8);

        // Checks if the commit file exists
        int exists = access(commitPath, F_OK);
        if(exists == -1) {
            printf("ERROR: .Commit file does not exist\n");
            cleanUp();
            return -1;
        }

        int status = push(projName, commitPath);
        cleanUp();
        free(commitPath);
        return status;
    }


    return 0;
}