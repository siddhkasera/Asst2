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

char* hostname;
char* port;
int network_socket;

typedef struct file {
    char* filePath;
    char* hash;
    int fileVersion;
    struct file* next;
}file;


int setConfigure() {

    int fd = open(".configure", O_RDONLY);

    // Check if file opened properly
    if(fd == -1){
        printf("ERROR: %s\n", strerror(errno));
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
    buffer[bytesread] = '\0';
    close(fd);
    return 0;
    
}

void cleanUp(){
    close(network_socket);
    free(hostname);
    free(port);
}

int createAndDestroy(char* action, char* project){
    // Write to Server the Action performed and Name of Project
    write(network_socket, action, strlen(action));
    write(network_socket, "@", sizeof(char));
    write(network_socket, project, strlen(project));
    write(network_socket, "@", sizeof(char));

    char actions[8];
    int bytesread = 0;

    // Read Server response back
    while(bytesread < 9) {
        if(read(network_socket, &actions[bytesread], 1) < 0){
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
            fileSize +=atoi (buffer);
        }

        int bytesread = 0;

        char* fileData = malloc(fileSize*sizeof(char));
        if(fileData == NULL){
            cleanUp();
            printf("ERROR: %s", strerror(errno));
            return -1;
        }
        
        // Read in Manifest Data
        int i;
        for(i = 0; i < fileSize; i++){
            if(read(network_socket, &fileData[bytesread], 1) < 0){
                free(fileData);
                cleanUp();
                printf("ERROR: %s\n", strerror(errno));
                return -1;
            }
            bytesread++;
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
        chmod(filePath, S_IRWXU);

        write(fd, fileData, fileSize);
        close(fd);
        printf("Project Successfully Created\n");
        return 0;
    }
    printf("%s\n", actions);
    return 0;
}

void freeFiles(file* files, int num){
    int i;
    file* ptr = files;
    for(i = 0; i < num; i++) {
        free(ptr ->filePath);
        free(ptr -> hash);
        file* temp = ptr;
        ptr = ptr -> next;
        free(temp);
    }
}

char* createHash(char* filePath) {
    int fd = open(filePath, O_RDONLY);
    int size = 100;
    char* data = malloc(100*sizeof(char));
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
        if(readstatus == 100) {
            size *= 2;
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
    if(bytesread == 0){
        char* hash = calloc(161, sizeof(char));
        hash[160] = '\0';
        return hash;
    }
    char* hash = malloc(161*sizeof(char));
    SHA1(data, sizeof(data), hash);
    hash[160] = '\0';

    return hash;
}

 void reverse(char s[])
 {
     int i, j;
     char c;
 
     for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
         c = s[i];
         s[i] = s[j];
         s[j] = c;
     }
 }

 void itoa(int n, char s[])
 {
     int i, sign;
 
     if ((sign = n) < 0)  /* record sign */
         n = -n;          /* make n positive */
     i = 0;
     do {       /* generate digits in reverse order */
         s[i++] = n % 10 + '0';   /* get next digit */
     } while ((n /= 10) > 0);     /* delete it */
     if (sign < 0)
         s[i++] = '-';
     s[i] = '\0';
     reverse(s);
 }

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
    
    // Read in Manifest Version Number
    int ver = 0;
    int readstatus = 1;
    int fd = open(manifest, O_RDONLY);
    if(fd == -1) {
        free(manifest);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }
    char buffer[2];
    buffer[1] = '\0';
    
    readstatus = read(fd, buffer, 1);
    if(readstatus == -1){
        free(manifest);
        close(fd);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    ver = atoi(buffer);
    while(1) {
        readstatus = read(fd, buffer, 1);
        if(readstatus == -1){
            free(manifest);
            close(fd);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        if(buffer[0] == '\n') {
            printf("hello\n");
            break;
        }
        ver *= 10;
        ver += atoi(buffer);
    }

    // Read in all entries
    int found = 0;
    file* files;
    file* ptr = files;
    int num = 0;
    while(1) {

        // Read Status
        readstatus = read(fd, &buffer, 1);
        if(readstatus == 0){
            break;
        }
        if(readstatus == -1) {
            printf("ERROR: %s\n", strerror(errno));
            free(manifest);
            close(fd);
            freeFiles(files, num);
            return -1;
        }

        // Create an entry
        file* newFile = malloc(sizeof(file));
        if(newFile == NULL) {
            printf("ERROR: %s\n", strerror(errno));
            free(manifest);
            freeFiles(files, num);
            close(fd);
            return -1;
        }
        if(num == 0) {
            files = newFile;
            ptr = files;
        } else {
            ptr -> next = newFile;
        }
        
        // Read in File Version
        newFile -> fileVersion = atoi(buffer);

        while(1) {
            read(fd, &buffer, 1);
            if(readstatus == -1) {
                printf("ERROR: %s\n", strerror(errno));
                free(manifest);
                close(fd);
                ptr -> next = NULL;
                free(newFile);
                freeFiles(files, num);
                return -1;
            }
            if(buffer[0] == ' ') {
                break;
            }
            ptr -> fileVersion *= 10;
            ptr -> fileVersion += atoi(buffer);
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
                freeFiles(files, num);
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
                    free(newFile);
                    ptr -> next = NULL;
                    freeFiles(files, num);
                    free(temp);
                    printf("ERROR: %s\n", strerror(errno));
                    return -1;
                }
                memcpy(filePath, temp, bytesread + 1);
                free(temp);
            }
            bytesread++;
        }

        newFile -> filePath = filePath;

        // If Current File matches Request File
        if(strcmp(filePath, fileName) == 0){
            found = 1;
            if(strcmp(argv[1], "add") == 0){
                newFile -> hash = createHash(filePath);
                if(errno != 0) {
                    free(filePath);
                    free(newFile);
                    ptr -> next = NULL;
                    freeFiles(files, num);
                    return -1;
                }
                continue;
            } else {
                free(newFile -> filePath);
                ptr -> next = NULL;
                free(newFile);
                char* useless[161];
                read(fd, useless, 161);
                continue;
            }
        }
        
        // Read in Hash
        char* hash = malloc(161*sizeof(char));
        readstatus = read(fd, hash, 161);
        if(readstatus == -1){
            free(manifest);
            free(hash);
            freeFiles(files, num);
            close(fd);
            printf("ERROR: %s\n", strerror(errno));
            return -1;
        }
        hash[20] = '\0';

        newFile -> hash = hash;
        
        ptr = ptr -> next;
        num++;
        
    }
    printf("HELLO\n");
    // If File was not found in the manifest
    if(found == 0){
        if(strcmp(argv[1], "remove") == 0) {
            printf("Warning: No such file exists in .manifest\n");
            return 0;
        }
        if(num == 0) {
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
        num++;
    }
    printf("HELLO\n");
    close(fd);
    fd = open(manifest, O_WRONLY);
    free(manifest);
    if(fd == -1) {
        freeFiles(files, num);
        printf("ERROR: %s\n", strerror(errno));
        return -1;
    }

    int space = 0;
    int temp = ver;
    if(ver == 0) {
        space = 1;
    }
    while(temp != 0){
        temp/=10;
        space++;
    }
    char* number = malloc((space+1)*sizeof(char));
    itoa(ver, number);
    number[space] = '\0';
    write(fd, number, strlen(number));
    write(fd, "\n", 1);
    ptr = files;
    int i;
    for(i = 0; i < num; i++) {
        space = 0;
        temp = ptr -> fileVersion;
        if(temp == 0) {
            space = 1;
        }
        while(temp != 0){
            temp/=10;
            space++;
        }
        char* verString = malloc((space+1)*sizeof(char));
        itoa(ptr -> fileVersion, verString);
        verString[space] = '\0';
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
    
    // If user wants to create or destroy a project ...
    if(strcmp("create", argv[1]) == 0 || strcmp("destroy", argv[1]) == 0) {
        return createAndDestroy(argv[1], argv[2]);
    }
    
    // Close socket
    close(network_socket);

    return 0;
}