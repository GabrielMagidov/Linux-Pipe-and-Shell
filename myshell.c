#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include "LineParser.h"
#include "myshell.h"

void printStatus(int n){
    if(n == 0){
        printf("%s  ", "SUSPENDED");
    }
    else if(n == 1){
        printf("%s  ", "RUNNING");
    }
    else if(n == -1){
        printf("%s  ", "TERMINATED");
    }
}

void addProcess(process** process_list, cmdLine* cmd, pid_t pid){
    process *newP = (process*) malloc(sizeof(struct process));
    newP->cmd = cmd;
    newP->pid = pid;
    newP->status = RUNNING;
    newP->next = NULL;
    if(*process_list == NULL){
       *process_list = newP;
    }
    else{
        process *nextP = (*process_list);
        while (nextP->next != NULL){
            nextP = nextP->next;
        }
        nextP->next = newP;
    }
    newP=NULL;
}

void printProcessList(process** process_list){
    if(process_list != NULL){
        updateProcessList(process_list);
        process *current = (*process_list);
        process *prev = NULL;
        int i = 0;
        printf("%s\n","Index   PID   STATUS     Command");
        while (current != NULL){
            printf("  %d    ", i);
            printf("%d  ", current->pid);
            printStatus(current->status);
            printf(" %s ", current->cmd->arguments[0]);
            int j = 1;
            while(current->cmd->arguments[j] != NULL){
                printf("%s ", current->cmd->arguments[j]);
                j++;
            }
            printf("\n");
            if(current->status == TERMINATED){
                if(prev == NULL){
                    (*process_list) = current->next;
                    if(current->cmd != NULL){
                        freeCmdLines(current->cmd);
                    }
                    free(current);
                    current = *process_list;
                }
                else{
                    prev->next = current->next;
                    if(current->cmd != NULL){
                        freeCmdLines(current->cmd);
                    }
                    free(current);
                    current = prev->next;
                }
            }
            else{
                prev = current;
                current = current->next;
            }
            i++;
        }
        current = NULL;
        prev = NULL;
    }
}

void freeProcessList(process* process_list){
    if(process_list != NULL){
        process *current = process_list;
        process *next = process_list;
        while (next != NULL){
            if(current->cmd != NULL){
                freeCmdLines(current->cmd);
            }
            next = current->next;
            free(current);
            current = next;
        }
        current = NULL;
        next = NULL;
    }    
}

void updateProcessList(process **process_list)
{
    process *current = *process_list;
    while(current != NULL){
        
        pid_t pid = current->pid;
        int status = 0;
        int result = waitpid(pid, &status, WNOHANG);
        if(current->status != SUSPENDED){
            if(result == 0){
                current->status = RUNNING;
            }
            else if(result == -1){
                current->status = TERMINATED;
            }
            else{
                if(WIFEXITED(status) || WIFSIGNALED(status)){
                    current->status = TERMINATED;
                }
                else if(WIFSTOPPED(status)){
                    current->status = SUSPENDED;
                }
                else if(WIFCONTINUED(status)){
                    current->status = RUNNING;
                }
            }
        }
        current = current->next;
    }
}

void updateProcessStatus(process* process_list, int pid, int status){
    if(process_list != NULL){
        process *current = process_list;
        bool found = false;
        while (!found && current != NULL){
            if(current->pid == pid){
                found = true;
                if(current->status != SUSPENDED || status != TERMINATED){
                    current->status = status;
                }
            }
            current = current->next;
        }
        current = NULL;
    }
}

void freeHistory(char **history){
    for (int i = 0; i < HISTLEN; i++) {
        free(history[i]);
    }
    free(history);
}

int execute(cmdLine *pCmdLine, bool debugM, process** process_list, char **history){
        if(pCmdLine->next == NULL){
            int pid = fork();
            if(pid != 0 && debugM){
                printf("PID: %d\n", pid);
                printf("Executing command: %s\n", pCmdLine->arguments[0]);
            }
            if(pid == 0){
                if(pCmdLine->inputRedirect != NULL){
                    FILE *fIn = fopen(pCmdLine->inputRedirect, "r");
                    if(fIn == NULL){
                        perror("ERROR");
                        _exit(-1);
                        return -1;
                    }
                    else{
                        dup2(fileno(fIn),0);
                        fclose(fIn);
                    }
                }
                if(pCmdLine->outputRedirect != NULL){
                    FILE *fOut = fopen(pCmdLine->outputRedirect, "w");
                    if(fOut == NULL){
                        perror("ERROR");
                        _exit(-1);
                        return -1;
                    }
                    else{
                        dup2(fileno(fOut), 1);
                        fclose(fOut);
                    }
                }
                if(execvp(pCmdLine->arguments[0], pCmdLine->arguments) == -1){
                    perror("ERROR");
                    freeCmdLines(pCmdLine);
                    return -1;
                }
                return 0;
            }
            else{
                addProcess(process_list, pCmdLine, pid);
                if(pCmdLine->blocking == 1){
                    waitpid(pid,0,0);
                }
                return 0;
            }
        }
        else{
            if(pCmdLine->outputRedirect != NULL){
                perror("ERROR");
                _exit(-1);
                return -1;
            }
            else if(pCmdLine->next->inputRedirect != NULL){
                perror("ERROR");
                _exit(-1);
                return -1;
            }
            else{
                if(execute2(pCmdLine,debugM, process_list, history) == -1){
                    perror("ERROR");
                    freeCmdLines(pCmdLine);
                    _exit(-1);
                    return -1;
                }
                else{
                    return 0;
                }
            }
        }
    return 0;
}

int execute2(cmdLine *pCmdLine, bool debugM, process** process_list, char **history){
    int fiN = 0;
    int foN = 1;
    cmdLine *cmdNext = pCmdLine->next;
    pCmdLine->next = NULL;
    int fd[2] = {fiN, foN};
    pid_t pid1, pid2;
    if (pipe(fd) == -1) {
        perror("pipe");
        _exit(-1);
    }
    pid1 = fork();
    if (pid1 == -1) {
        perror("fork");
        _exit(-1);
    }
    if (pid1 == 0) {
        close(1);
        dup(fd[1]);
        close(fd[1]);
        if(execute(pCmdLine,debugM, process_list, history) == -1){
            perror("ERROR");
        }
        freeProcessList(*process_list);
        pCmdLine = NULL;
        freeCmdLines(cmdNext);
        freeHistory(history);
        exit(0);
    } else {
        close(fd[1]);
        addProcess(process_list,pCmdLine,pid1);
        pid2 = fork();
        if (pid2 == -1){
            perror("fork");
            _exit(-1);
        }
        if(pid2 == 0){
            close(0);
            dup(fd[0]);
            close(fd[0]);
            if(execute(cmdNext,debugM, process_list, history) == -1){
                perror("ERROR");
            }
            if(process_list != NULL){
                freeProcessList(*process_list);
            }
            freeHistory(history);
            cmdNext = NULL;
            exit(0);
        }
        else{
            close(fd[0]);
            addProcess(process_list,cmdNext,pid2);
            waitpid(pid1,0,0);
            waitpid(pid2,0,0);
            return 0;    
        }
    }
    return 0;
}

int NonProCommands(char *line, bool debugM, process **process_list, int newest, int oldest, int histSize, char **history){
    if(strncmp(line, "cd", 2) == 0){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", "cd");
        }
        if(chdir(line+2) == -1){
            perror("ERROR");
        }
        return 0;
    }
    else if(strncmp(line, "suspend", 7) == 0){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", "suspend");
        }
        if(kill(atoi(line+7),SIGTSTP) == -1){
            perror("ERROR");
        }
        else{
            printf("handling SIGTSTP\n");
            updateProcessStatus(*process_list,atoi(line+7),SUSPENDED);
            }
        return 0;
    }   
    else if(strncmp(line, "wake", 4) == 0){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", "wake");
        }
        if(kill(atoi(line+4),SIGCONT) == -1){
            perror("ERROR");
        }
        else{
            printf("handling SIGCONT\n");
            updateProcessStatus(*process_list,atoi(line+4),RUNNING);
        }
        return 0;
    }
    else if(strncmp(line, "kill", 4) == 0){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", "kill");
        }
        if(kill(atoi(line+4),SIGINT) == -1){
            perror("ERROR");
        }
        else{
            printf("handling SIGINT\n");
            updateProcessStatus(*process_list,atoi(line+4),TERMINATED);
        }
        return 0;
    }
    else if(strncmp(line, "history", 7) == 0){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", line);
        }
        int num = atoi(line+8);
        if(num >= HISTLEN){
            num = HISTLEN;
        }
        int j = (newest - num + 1) % HISTLEN;
        if(num >= histSize){
            j = oldest;
            num = histSize;
        }
        for(int k = 0; k < num; k++){
            if(strncmp(history[j], "\n", 1) != 0){
                printf("%s", history[j]);
            }
            j = (j + 1) % HISTLEN;
        }
        return 0;
    }
    else if(strncmp(line, "!!", 2) == 0 && (strncmp(line+2, "\n", 1) == 0)){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", line);
        }
        if(histSize >= 1){
            strcpy(line,history[newest]);
            int ans = NonProCommands(line,debugM,process_list,newest,oldest,histSize,history);
            if(ans == 1){
                struct cmdLine *cLine = parseCmdLines(line);
                if(execute(cLine, debugM, process_list, history) == -1){
                    return 2;
                }  
            }
            else{
                return ans;
            }
        }
        else{
            printf("%s\n","Error: invalid number (no such entry yet)");
            return 2;
        }
        return 0;
    }
    else if(strncmp(line, "!", 1) == 0 && (strncmp(line+1, "\n", 1) != 0)){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", line);
        }
        int num = atoi(line+1);
        if(num < 0 || num >= HISTLEN){
            printf("%s\n","Error: invalid number (out of range)");
            return 2;
        }
        else if(num > histSize){
            printf("%s\n","Error: invalid number (no such entry yet)");
            return 2;
        }
        else{
            strcpy(line,history[(newest - num + 1) % HISTLEN]);
            int ans = NonProCommands(line,debugM,process_list,newest,oldest,histSize,history);
            if(ans == 1){
                struct cmdLine *cLine = parseCmdLines(line);
                if(execute(cLine, debugM, process_list,history) == -1){
                    return 2;
                }  
            }
            return ans;
        }
        return 0;
    }
    else if(strncmp(line, "procs", 5) == 0 && (strncmp(line+5, "\n", 1) == 0)){
        if(debugM){
            printf("PID: %d\n", 0);
            printf("Executing command: %s\n", line);
        }
        printProcessList(process_list);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv){
    process* pl = NULL;
    process** process_list = &(pl);
    bool debugM = false;
    bool ignore = false;
    if(argc >= 2 && strncmp(argv[1], "-d", 2) == 0){
        debugM = true;
    }
    bool stop = false;
    char line[2048];
    char **history = malloc(HISTLEN * sizeof(char *));
    int histSize = 0;
    int newest = 0;
    int oldest = 0;
    for (int i = 0; i < HISTLEN; i++) {
        history[i] = malloc(2048 * sizeof(char));
    }
    while(!stop){
        ignore = false;
        printf("<");
        fgets(line, 2048, stdin);
        if((strncmp(line, "quit", 4) == 0) && (strncmp(line+4, "\n", 1) == 0)){
            stop = true;
            break;
        }
        int ans = NonProCommands(line,debugM,process_list,newest,oldest,histSize,history);
        if(!stop && ans == 1){
            if(strncmp(line, "\n", 1) != 0){
                struct cmdLine *cLine = parseCmdLines(line);
                if(execute(cLine, debugM, process_list,history) == -1){
                    stop = true;
                    ignore = true;
                    if(*process_list != NULL){
                       freeProcessList(*process_list); 
                    }
                    break;
                }
                else{
                    stop = false;
                }
            } 
        }
        if(ans == 2){
            ignore = true;
        }
        if(!ignore){
            if(histSize == 20 || newest+1 == HISTLEN){
                strcpy(history[oldest],line);
                newest = oldest;
                oldest = (oldest + 1) % HISTLEN;
            }
            else if(histSize == 0){
                strcpy(history[0],line);
                histSize++;
            }
            else{
                newest++;
                strcpy(history[newest],line);
                histSize++;
            }
        }
    }
    freeProcessList(*process_list);
    freeHistory(history);
    return 0;
}