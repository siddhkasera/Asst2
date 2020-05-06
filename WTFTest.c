#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h> 

char* readFromFile(char* filePath){
    int fd = open(filePath, O_RDONLY);
    int size = 101;
    char* data = malloc(101*sizeof(char));
    int bytesread = 0;
    int readstatus = 1;
    while(readstatus > 0) {
        readstatus = read(fd, &data[bytesread], 100);
        bytesread += readstatus;
        if(bytesread == size - 1) {
            size *= 2;
            size--;
            char* temp = data;
            data = malloc(size*sizeof(char));
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
    return data;

}

int main(char* argc, char* argv[]){

    printf("Test 1: Configuration\n");
    int process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF configure localhost 5000");
        char* configure = readFromFile(".configure");
        if(strcmp(configure, "localhost 5000") == 0) {
            printf("\nTest Succeeded!\n");
        } else {
            printf("\nFailure: .configure had incorrect data\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 2: Create a Project\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF create test");
        if(access("test", F_OK) == 0 && access("test/.manifest", F_OK) == 0) {
            if(access("server/test", F_OK) == 0 && access("server/test/.manifest", F_OK) == 0 && access("server/test/.data", F_OK) == 0 && access("server/test/.history", F_OK) == 0){
                char* manifestData = readFromFile("test/.manifest");
                if(strcmp(manifestData, readFromFile("server/test/.history")) == 0 && strcmp(manifestData, readFromFile("server/test/.manifest")) == 0) {
                    printf("\nTest Succeeded!\n");
                } else {
                    printf("\nFailure: Files don't have correct data\n");
                }
            } else {
                printf("\nFailure: Server does not have all files\n");
            }
        } else {
            printf("\nFailure: Client does not have all files\n");
        }
        return 0;
    }
    wait(&process);
    system("rm -rf test");

    printf("\nTest 3: Checkout a Project\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF checkout test");
        if(access("test", F_OK) == 0 && access("test/.manifest", F_OK) == 0) {
            char* manifestData = readFromFile("test/.manifest");
            if(strcmp(manifestData, readFromFile("server/test/.manifest")) == 0) {
                printf("\nTest Succeeded!\n");
            } else {
                printf("\nFailure: Files don't have correct data\n");
            }
        } else {
            printf("\nFailure: Client does not have all files\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 4: Destroy a Project\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF destroy test");
        if(access("server/test",F_OK) != 0) {
            printf("\nTest Succeeded!\n");
        } else {
            printf("\nFailure: Project still exists on server side\n");
        }
        return 0;
    }
    wait(&process);
    system("rm -rf test");

    printf("\nTest 5: Add to a Project\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF create test");
        int fd = open("test/test.txt", O_CREAT | O_RDWR, 0777);
        close(fd);
        system("./WTF add test test.txt");
        if(strcmp(readFromFile("test/.manifest"), "0\n0 test/test.txt DA39A3EE5E6B4B0D3255BFEF95601890AFD80709\n") == 0 ) {
            printf("\nTest Succeeded!\n");
        } else {
            printf("\nFailure: .manifest contents are incorrect\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 6: Remove from a Project\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF remove test test.txt");
        if(strcmp(readFromFile("test/.manifest"), "0\n") == 0) {
            printf("\nTest Succeeded!\n");
        } else {
            printf("\nFailure: .manifest contents are incorrect\n");
        }
        return 0;
    }
    wait(&process);
    
    printf("\nTest 7: Commit Changes\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF add test test.txt");
        system("./WTF commit test");
        if(access("test/.Commit", F_OK) == 0) {
            char* commitData = readFromFile("test/.Commit");
            if(strcmp(commitData, "A test/test.txt DA39A3EE5E6B4B0D3255BFEF95601890AFD80709\n")) {
                if(access("server/test/.data/commit/.CommitB6C867A93FE36AB62A39A360946CC3515F730F88", F_OK) == 0) {
                    if(strcmp(commitData, readFromFile("server/test/.data/commit/.CommitB6C867A93FE36AB62A39A360946CC3515F730F88")) == 0) {
                        printf("\nTest Succeeded!\n");
                    } else {
                        printf("\nFailure: .Commit file on Server Side has incorrect data\n");
                    }
                } else {
                    printf("\nFailure: .Commit file does not exist in Server\n");
                }
            } else {
                printf("\nFailure: .Commit file has incorrect data\n");
            }
        } else {
            printf("\nFailure: .Commit file does not exist\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 8: Push Changes\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF push test");
        if(access("server/test/test.txt", F_OK) == 0 && access("server/test/.data/test0.tar", F_OK) == 0) {
            char* manifestData = readFromFile("test/.manifest");
            if(strcmp(manifestData, "1\n0 test/test.txt DA39A3EE5E6B4B0D3255BFEF95601890AFD80709\n") == 0 && strcmp(manifestData, readFromFile("server/test/.manifest")) == 0) {
                printf("\nTest Succeeded!\n");
            } else {
                printf("\nFailure: Manifest Files are not updated\n");
            }
        } else {
            printf("\nFailure: Server does not have correct files\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 9: Current Version\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF currentversion test");
        printf("\nTest Succeeded!\n");
        return 0;
    }
    wait(&process);

    printf("\nTest 10: Rollback to Version 0\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF rollback test 0");
        if(access("server/test/test.txt", F_OK) != 0 && access("server/test/.data/test0.tar", F_OK) != 0) {
            char* manifestData = readFromFile("server/test/.manifest");
            if(strcmp(manifestData, "0\n") == 0) {
                printf("\nTest Succeeded!\n");
            } else {
                printf("\nFailure: Manifest Files are not updated\n");
            }
        } else {
            printf("\nFailure: Server does not have correct files\n");
        }        
        return 0;
    }
    wait(&process);
    
    printf("\nTest 11: History\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF history test");
        if(strcmp(readFromFile("server/test/.history"), "0\n\n1\nA test/test.txt DA39A3EE5E6B4B0D3255BFEF95601890AFD80709 0\n\nRollback to 0\n") == 0){
            printf("\nTest Succeeded!\n");
        } else {
            printf("\nFailure: History had wrong data\n");
        }
        return 0;
    }
    wait(&process);

    printf("\nTest 12: Update Client\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF update test");
        if(access("test/.Update", F_OK) == 0) {
            if(strcmp(readFromFile("test/.Update"), "D test/test.txt DA39A3EE5E6B4B0D3255BFEF95601890AFD80709\n") == 0) {
                printf("\nTest Succeeded!\n");
            } else {
                printf("Failure: .Update contents are incorrect\n");
            }
        } else {
            printf("Failure: .Update does not exist\n");
        }
        return 0;
    }
    wait(&process);
    
    printf("\nTest 13: Upgrade Client\n");
    process = fork();
    if(process == -1) {
        printf("Error in forking, stopping tests\n");
    }
    if(!process) {
        system("./WTF upgrade test");
        if(access("test/.Update", F_OK) != 0) {
            if(strcmp(readFromFile("test/.manifest"), "0\n") == 0) {
                printf("\nTest Succeeded!\n");
            } else {
                printf("Failure: Manifest file did not update\n");
            }
        } else {
            printf("Failure: .Update still exists\n");
        }
        return 0;
    }
    wait(&process);
    
    if(access("test", F_OK) == 0) {
        system("rm -rf test");
    }
    if(access("server/test", F_OK) == 0) {
        system("rm -rf server/test");
    }
    

    return 0;
}