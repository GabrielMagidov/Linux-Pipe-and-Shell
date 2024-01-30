#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
    printf("%s\n", "Hello World!");
    int fd[2];
    pid_t pid1, pid2;
    if (pipe(fd) == -1) {
        perror("pipe");
        _exit(-1);
    }
    printf("%s\n", "parent_process>forking");
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        _exit(-1);
    }
    if(pid1 != 0){
        printf("parent_process>created process with id: %d\n", pid1);
    }
    if (pid1 == 0) { //this is child1 process
        close(1);
        printf("%s\n", "child1>redirecting stdout to the write end of the pipe");
        dup(fd[1]);
        close(fd[1]);
        char* args1[] = {"ls", "-l", NULL};
        printf("%s\n", "child1>going to execute cmd: ls-l");
        if(execvp(args1[0], args1) == -1){
            perror("ERROR");
            _exit(-1);
            return -1;
        }
        _exit(-1);
    } else {
        printf("%s\n", "parent_process>closing the write end of the pipe");
        close(fd[1]);
        printf("%s\n", "parent_process>forking");
        pid2 = fork();
        if (pid2 == -1){
            perror("fork");
            _exit(-1);
        }
        if(pid2 != 0){
        printf("parent_process>created process with id: %d\n", pid2);
        }
        if(pid2 == 0){
            close(0);
            printf("%s\n", "child2>redirecting stdin to the read end of the pipe");
            dup(fd[0]);
            close(fd[0]);
            char* args2[] = {"tail", "-n", "2", NULL};
            printf("%s\n", "child2>going to execute cmd: tail -n 2");
            if(execvp(args2[0], args2) == -1){
                perror("ERROR");
                _exit(-1);
                return -1;
            }
            exit(0);
        }
        else{
            printf("%s\n", "parent_process>closing the read end of the pipe");
            close(fd[0]);
            printf("%s\n", "parent_process>waiting for child processes to terminate");
            waitpid(pid1,0,0);
            waitpid(pid2,0,0);
            printf("%s\n", "parent_process>exiting");
            exit(0);    
        }
    }
}