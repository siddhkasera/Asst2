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
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

typedef struct file {
    char* status;
    char* filePath;
    char* hash;
    int fileVersion;
    struct file* next;
}file;

char* hostname;
char* port;
int network_socket;
int sVer;
int cVer;

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
    close(network_socket);
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

    // Check if there was an error with connection
    if(connection_status == -1){
        cleanUp();
        printf("ERROR: Could not make connection to the remote socket");
        return -1;
    }

    return 0;
}

// Read Server Response 
char* serverResponse() {
    char* actions = malloc(8*sizeof(char));
    int bytesread = 0;
    while(bytesread < 9) {
        if(read(network_socket, &actions[bytesread], 1) < 0){
            cleanUp();
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

// Read in Data Size from Server
int dataSize(){
    char buffer[2];
    buffer[1] = '\0';
    int fileSize = 0;
    read(network_socket, buffer, 1);
    fileSize = atoi(buffer);

    while(1){
        if(read(network_socket, buffer, 1) < 0){
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

// Get File Data from server
char* retrieveData(int fileSize){
    char* fileData = malloc((fileSize+1)*sizeof(char));
    if(fileData == NULL){
        cleanUp();
        printf("ERROR: %s", strerror(errno));
        return NULL;
    }
    
    int i;
    for(i = 0; i < fileSize; i++){
        if(read(network_socket, &fileData[i], 1) < 0){
            free(fileData);
            cleanUp();
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
    int fd = open(manifest, O_CREAT | O_WRONLY);
    chmod(manifest, 777);
    free(manifest);

    if(fd == -1) {
        freeFiles(files);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    char* number = intSpace(cVer);
    write(fd, number, strlen(number));
    write(fd, "\n", 1);
    file* ptr = files;

    int i;
    while(ptr != NULL) {
        char* verString = intSpace(ptr -> fileVersion);
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

// Create or Destroy a project
int createAndDestroy(char* action, char* project){
    // Write to Server the Action performed and Name of Project
    write(network_socket, action, strlen(action));
    write(network_socket, "@", sizeof(char));
    write(network_socket, project, strlen(project));
    write(network_socket, "@", sizeof(char));

    // Read Server response back
    char* actions = serverResponse(network_socket);

    // If there was an error...
    if(strcmp(actions, "ERROR") == 0) {
        cleanUp();
        if(strcmp("create", action) == 0) {
            printf("ERROR: Project Already Exists\n");
        } else {
            printf("ERROR: Project Doesn't Exist\n");
        }
        return -1;
    }
    
    // If the server is sending the manifest file...
    else if(strcmp(actions, "sending") == 0) {

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

        // Make the Project Directory
        if(mkdir(project, 777) < 0) {
            cleanUp();
            free(fileData);
            printf("ERROR: Could not make project directory\n");
            return -1;
        }

        // Make the Manifest File  
        char filePath[strlen(project) + 11];
        memcpy(filePath, project, strlen(project));
        memcpy(&filePath[strlen(project)], "/", 1);
        memcpy(&filePath[strlen(project) + 1], ".manifest", 10);

        int fd = open(filePath, O_RDWR | O_CREAT);
        if(fd == -1){
            cleanUp();
            free(fileData);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        chmod(filePath, 777);

        write(fd, fileData, fileSize);
        close(fd);
        printf("Project Successfully Created\n");
        return 0;
    }
    printf("%s\n", actions);
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
    file* files;
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
            if(strcmp(actions, "add") == 0){
                newFile -> hash = createHash(filePath);
                if(errno != 0) {
                    free(filePath);
                    free(newFile);
                    freeFiles(files);
                    return NULL;
                }
            } else {
                free(newFile -> filePath);
                free(newFile);
                char* useless[41];
                read(fd, useless, 41);
                continue;
            }
            char* useless[41];
            read(fd, useless, 41);
        } else {
            // Read in Hash
            char* hash = malloc(41*sizeof(char));
            readstatus = read(fd, hash, 41);
            if(readstatus == -1){
                free(hash);
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
            return files;
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
    return files;
}

// Produces a .Update file for users on client side
int update(char* projName, char * updatePath, char * manifestPath){
    write(network_socket, "update@", 7);
    write(network_socket, manifestPath, strlen(manifestPath));
    write(network_socket,"@",1);

    char* actions = serverResponse();
    if(strcmp(actions, "ERROR") == 0) {
        printf("ERROR: File Did not exist on server\n");
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
        cleanUp();
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
        } else if(strcmp(clientPtr -> hash, serverPtr -> hash) != 0) {
            char* liveHash = createHash(clientPtr -> filePath);
            if(strcmp(liveHash, clientPtr -> hash) != 0) {
                if(conflictFile == -1) {
                    conflictFile = open(conflictPath, O_CREAT | O_RDWR);
                    chmod(conflictPath, 777);
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
    cleanUp();
    return 0;
}

// Gets Current Version and status of a project
int currVer(char* projectName, char* manifestPath){
    write(network_socket, "currVer@", 8);
    write(network_socket, manifestPath, strlen(manifestPath));
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

// Upgrade files in client side 
int upgrade(char* manifestPath, char* updatePath) {
    int manifestFile = open(manifestPath, O_RDONLY);
    file* files = readManifest(manifestFile, "upgrade", NULL, 1);
    close(manifestFile);
    if(files == NULL) {
        close(manifestFile);
        return -1;
    }

    int empty = 1;
    int updateFile = open(updatePath, O_RDONLY);

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
                freeFiles(files);
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
                    freeFiles(files);
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
            freeFiles(files);
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

            if(strcmp(actions, "sending") == 0) {
                
                // Read in size of data
                int fileSize = dataSize();
                
                // Read in Data from Server
                char* fileData = retrieveData(fileSize);
                remove(filePath);
                int file = open(filePath, O_CREAT | O_RDWR);
                chmod(filePath, 777);
                write(file, fileData, fileSize);
                close(file);
            }

        }

        file* prev = NULL;
        file* ptr = files;

        while(ptr != NULL) {
            if(strcmp(ptr -> filePath, filePath) == 0) {
                if(action[0] == 'D') {
                    file* temp;
                    if(prev == NULL) {
                        temp = files;
                        files = files -> next;
                    }
                    else {
                        temp = ptr;
                        prev -> next = ptr -> next;
                    }
                    free(temp -> filePath);
                    free(temp -> hash);
                    free(temp);
                } else {
                    free(ptr -> hash);
                    ptr -> hash = hash;
                    ptr -> fileVersion = ptr ->fileVersion + 1;
                }
                break;
            }
            prev = ptr;
            ptr =  ptr -> next;
        }
        if(action[0] == 'A') {
            file* newFile = malloc(sizeof(file));
            newFile ->filePath =filePath;
            newFile -> hash = hash;
            newFile -> next = NULL;
            prev -> next = newFile;
        }

    }
    remove(updatePath);
    if(empty) {
        printf("Project is Up to Date!\n");
        return 0;
    }
    cVer++;
    write(network_socket, "done@", 5);
    return updateManifest(manifestPath, files);
}

// Checkout a Project from the server
int checkout(char* projName) {
    write(network_socket, "checkout@", 9);
    write(network_socket, projName, strlen(projName));
    write(network_socket, "@", 1);

    mkdir(projName, 777);
    int bytesread = 0;

    // Read Server response back
    char* actions = serverResponse();

    if(strcmp(actions, "ERROR") == 0) {
        printf("ERROR: Project Did not exist on server\n");
        return -1;
    }

    int numOfDir;
    char buffer[2];
    buffer[1] = '\0';
    
    // Read From Server Number of Directories
    numOfDir = dataSize();

    int i;
    for(i = 0; i < numOfDir; i++) {
        int size = 100;
        char* dirName = malloc(100*sizeof(char));
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
                memcpy(dirName, temp, bytesread);
                free(temp);
            }
        }
        mkdir(dirName, 777);
    }

    int numOfFiles = dataSize();

    for(i = 0; i < numOfFiles; i++) {
        int size = 100;
        char* fileName = malloc(100*sizeof(char));
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
                memcpy(fileName, temp, bytesread);
                free(temp);
            }
        }
        int fd = open(fileName, O_CREAT | O_RDWR);
        chmod(fileName, S_IRWXU);

        int fileSize = dataSize();
    
        char* fileData = retrieveData(fileSize);

        write(fd, fileData, fileSize);
        read(network_socket, buffer, 1);
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
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    file* files = readManifest(fd, argv[1], fileName, 1);
    if(files == NULL) {
        close(fd);
        return -1;
    }
    close(fd);

    return updateManifest(manifest, files);

}


int main(int argc, char* argv[]) { 

    // Check for Minimum Number of Arguments
    if(argc < 3) {
        printf("ERROR: Incorrect Number of Arguments\n");
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
        if(atoi(argv[3]) > 65535 || atoi(argv[3]) == 0) {
            printf("ERROR: Port must be a number that is less than or equal to 65535\n");
            return -1;
        }

        // Create/Rewrite configure file
        remove(".configure");
        int fd = open(".configure", O_CREAT | O_RDWR, 777);
       

        // Check if file opened properly
        if(fd == -1){
            printf("ERROR: %s\n", strerror(errno));
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
    
    if(strcmp(argv[1], "add") == 0 || strcmp(argv[1], "remove") == 0) {
        // Check for Correct Number of Arguments
        if(argc != 4){
            printf("ERROR: Incorrect Format, expected: ./WTF <action> <projectName> <filename>\n");
            return -1;
        }

        //Check if Project exists
        int exists = access(argv[2], F_OK);
        if(exists == -1) {
            printf("ERROR: Project does not exist\n");
            return -1;
        }

        char* fileName = malloc((strlen(argv[2]) + strlen(argv[3] + 2))*sizeof(char));
        memcpy(fileName, argv[2], strlen(argv[2]));
        memcpy(&fileName[strlen(argv[2])], "/", 1);
        memcpy(&fileName[strlen(argv[2]) + 1], argv[3], strlen(argv[3]));

        //Check if filename exists
        exists = access(fileName, F_OK);
        if(exists == -1) {
            printf("ERROR: File does not exist\n");
            return -1;
        }

        return addOrRemove(argv, fileName);
    }
    
    // If user wants to create or destroy a project ...
    if(strcmp("create", argv[1]) == 0 || strcmp("destroy", argv[1]) == 0) {
        connectServer();
        return createAndDestroy(argv[1], argv[2]);
    }
    
    // If user wants to checkout a project...
    if(strcmp("checkout", argv[1]) == 0) {
        connectServer();
        if(access(argv[2], F_OK) != -1) {
            printf("ERROR: Project already exists on client\n");
            cleanUp();
            return -1;
        }
        return checkout(argv[2]);
    }

    //If user wants to update a project...
    if(strcmp("update", argv[1]) == 0) {
        connectServer();

        if(access(argv[2], F_OK) == -1) {
            printf("ERROR: Project does not exist on client\n");
            return -1;
        }

        // Make Update Path
        char* updatePath = malloc((strlen(argv[2]) + 9)*sizeof(char));
        memcpy(updatePath, argv[2], strlen(argv[2]));
        memcpy(&updatePath[strlen(argv[2])], "/", 1);
        memcpy(&updatePath[strlen(argv[2]) + 1], ".Update", 8);

        // Make Manifest Path
        char* manifestPath = malloc((strlen(argv[2]) + 11)*sizeof(char));
        memcpy(manifestPath, argv[2], strlen(argv[2]));
        memcpy(&manifestPath[strlen(argv[2])], "/", 1);
        memcpy(&manifestPath[strlen(argv[2]) + 1], ".manifest", 10);

        return update(argv[2], updatePath, manifestPath);
    }    

    // If user wants to upgrade a project...
    if(strcmp("upgrade", argv[1]) == 0) {
        connectServer();
        if(access(argv[2], F_OK) == -1) {
            printf("ERROR: Project does not exist on client\n");
            return -1;
        }

        char* updatePath = malloc((strlen(argv[2]) + 11)*sizeof(char));
        memcpy(updatePath, argv[2], strlen(argv[2]));
        memcpy(&updatePath[strlen(argv[2])], "/", 1);
        memcpy(&updatePath[strlen(argv[2]) + 1], ".Conflict", 10);

        if(access(updatePath, F_OK) != -1) {
            printf("ERROR: Resolve all conflicts before update\n");
            return -1;
        }

        memcpy(&updatePath[strlen(argv[2]) + 1], ".Update", 7);
        updatePath[strlen(argv[2]) + 8]  = '\0';

        if(access(updatePath, F_OK) == -1) {
            printf("ERROR: Call update before calling upgrade\n");
            return -1;
        }
        
        char* manifestPath = malloc((strlen(argv[2]) + 11)*sizeof(char));
        memcpy(manifestPath, argv[2], strlen(argv[2]));
        memcpy(&manifestPath[strlen(argv[2])], "/", 1);
        memcpy(&manifestPath[strlen(argv[2]) + 1], ".manifest", 10);
        return upgrade(manifestPath, updatePath);
    }

    // If user wants the history of the project...
    if(strcmp(argv[1], "history") == 0) {
        connectServer();
        return history(argv[2]);
    }

    // If user wants the current version of the project...
    if(strcmp(argv[1], "currentversion") == 0) {
        connectServer();
        char* manifestPath = malloc((strlen(argv[2]) + 11)*sizeof(char));
        memcpy(manifestPath, argv[2], strlen(argv[2]));
        memcpy(&manifestPath[strlen(argv[2])], "/", 1);
        memcpy(&manifestPath[strlen(argv[2]) + 1], ".manifest", 10);
        return currVer(argv[2], manifestPath);
    }
    
    // Close socket
    close(network_socket);

    return 0;
}