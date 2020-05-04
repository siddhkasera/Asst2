#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

int main(char * argc, char * argv[]){
  
  int c1 = system("./WTF configure cd.cs.rutgers.edu 5000");
  if(c1 == -1){
    printf("Command configure  not executed\n");
  }
  int c2 = system("./WTF create test");
  if(c2 == -1){
    printf("Command create not executes\n");
  }


  return 0;
}
