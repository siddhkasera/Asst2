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

char* hostname;
char* port;
int network_socket;

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
        int size = 100;
        char* fileSize = malloc(100*sizeof(char));
        bytesread = 0;
        while(1){
            if(read(network_socket, &fileSize[bytesread], 1) < 0){
                free(fileSize);
                cleanUp();
                printf("ERROR: %s\n", strerror(errno));
                return -1;
            }
            if(fileSize[bytesread] == '@') {
                fileSize[bytesread] = '\0';
                break;
            }
            bytesread++;
            if(bytesread >= size -2) {
                char* temp = fileSize;
                size = size*=2;
                fileSize = malloc(size*sizeof(char));
                if(fileSize == NULL){
                    cleanUp();
                    free(temp);
                    printf("ERROR: %s", strerror(errno));
                    return -1;
                }
                memcpy(fileSize, temp, bytesread);
                free(temp);
            }
        }

        bytesread = 0;
        int bytesToRead = atoi(fileSize);
        free(fileSize);   

        char* fileData = malloc(bytesToRead*sizeof(char));
        if(fileData == NULL){
            cleanUp();
            printf("ERROR: %s", strerror(errno));
            return -1;
        }
        
        // Read in Manifest Data
        int i;
        for(i = 0; i < bytesToRead; i++){
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

        write(fd, fileData, bytesToRead);
        close(fd);
        printf("Project Successfully Created\n");
        return 0;
    }
    printf("%s\n", actions);
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
        memcpy(fileName[strlen(argv[2])], "/", 1);
        memcpy(fileName[strlen(argv[2]) + 1], argv[3], strlen(argv[3]));

        //Check if filename exists
        int exists = access(fileName, F_OK);
        if(exists == -1) {
            printf("ERROR: File does not exist\n");
            return -1;
        }

        // Read through Manifest file
        char* manifest = malloc(strlen(argv[2]) + 10);
        memcpy(fileName, argv[2], strlen(argv[2]));
        memcpy(fileName[strlen(argv[2])] , "/", 1 );
        memcpy(fileName[strlen(argv[2]) + 1] , ".manifest", 10 );


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