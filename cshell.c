#include "getword.h"
#include "cshell.h"
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

/*
metaFlag codes:
-1 == no metaFlag
0 == '<'
1 == '>'
2 == '>&'
3 == '&'
4 == '>>'
5 == '>>&'
6 == '|'
*/
int pipeFlag = 0;

int main(int argc, char *argv[]){
        //static char histaux[MAXHISTORY], histfiles[((7+(256*2))*9)];        //[0] == metaflag 0 or null,[1] == metaflag 1,2,4,5 or null [2] == & or null, [3] == pipe or no pipe.. max of 2 words follow, infile or outfile. plus two (and additional 1 for null) for the index of where (if theres a pipe) the second program is.
    char histaux[MAXHISTORY], histfiles[((7+(256*2))*9)];        //[0] == metaflag 0 or null,[1] == metaflag 1,2,4,5 or null [2] == & or null, [3] == pipe or no pipe.. max of 2 words follow, infile or outfile. plus two (and additional 1 for null) for the index of where (if theres a pipe) the second program is.
    char *argv1[MAXITEM+1];             //added a 1 so that there is room for a null terminator in case user uses max args, argv2 is
    int rdOut = 0, stderror = 0, rdIn = 0, argc1 = 0, parseCode = 0, amps = 0, prompt = 0, arg_fd = 0, scriptFlag = 0, history = 0, piper = 0;    //rdIn/Out are file descriptors for redirect. stderror is a flag for stderror redirect.
    int *rdO = &rdOut, *rdI = &rdIn, *arg = &argc1, *stde = &stderror, *amp = &amps, *script = &scriptFlag, *hist = &history, *pip = &piper;            //argc1 counts amount of args, parseCode is assigned numberic code from the parse function's
    signal(SIGTERM, sighandler);                                        //return value. amps is a flag for '&'. arg_fd is file descriptor for the file passed
    int percent = 1;
    if(argc > 1){
        scriptFlag = 1;
        if (-1 == (arg_fd = open(argv[1], O_RDONLY))){                //redirect input for prog if its fed an arg
            perror("Failed to open file passed in the command line.");
            parseCode = -1;
        }
        if(-1 == (dup2(arg_fd, 0))){
            perror("Failed to dup2 argv[1].");
            parseCode = -1;
        }
    }
    while(parseCode>-1){
        if(!(scriptFlag)){                //DONT PRINT PROMPTS IF STDIN IS REDIRECTED? I THINK
            if(parseCode == 0)                    //if newline dont increment
                printf("%%%d%% ", percent);
            else
                printf("%%%d%% ", ++percent);
        }
        parseCode = parse(argv1, rdI, rdO, arg, stde, amp, script, hist, pip, histaux, histfiles);    //call separate function, parse. pass pointers to the new argv1 array, rdI & rdO for out/in redirect, arg for arg count, stde (stderror ewsirect flag, and amp which is the Ampersand flag)
        if(parseCode == -1){                    //pCode == -1 means logout.. kill the children (signal handler for parent above)
            killpg(getpid(), SIGTERM);
            if(!(scriptFlag)) printf("shell terminated.\n");
            exit(0);
        }
        else if(parseCode > 0){        //pCode == 1 means successful parsing, and is now ready for fork n' execute. pCode == 2 means user enter "!!". So fork n exe
            history++;
            if(parseCode >= 10){
                if(parseCode == 10 || (parseCode-10) >= history){    //the value of parsecode minus 10 corresponds with the command's number (e.g. 15-10 == command %5%)
                    printf("Event not found.\n");            //modeling off of tcsh error code
                    parseCode = 0;                    //dont increment percent if user tries to use the history command on the current command (modeling off of tcsh)
                    history--;
                    continue;
                }
                else{
                    parseCode = specialparse(argv1, rdI, rdO, arg, stde, amp, parseCode-11, pip, histaux, histfiles);
                    if(parseCode == 3)
                        continue;
                }
            }
            if(strcmp(argv1[0], "cd") == 0 && strlen(argv1[0]) == 2){    //built in cd -- checks for valid aarg count next line
                if(argc1 > 2)
                    perror("Too many arguments for cd");
                else if(argc1 == 1){
                    if((chdir(getenv("HOME"))) == -1)         //no args so cd to home dir by using getenv
                        perror("Failed to change directory");
                }
                else {
                    if(strcmp(argv1[1], "$HOME") == 0 && strlen(argv[1]) == 5){    //this is same behavior as if user only entered "cd"
                        if((chdir(getenv("HOME"))) == -1)
                            perror("Failed to change directory");
                    }
                    else if((chdir(argv1[1])) == -1)                 //chdir to argv[1] send error if not valid
                        perror("Failed to change directory");
                }
            }
            //else if(!amps){                                    //if NO '&' at the end then do this block
            else{                                    //if NO '&' at the end then do this block
                int kidpid, grandpid, status;
                int fildes[2];
                fflush(stdout);
                if(-1 == (kidpid = fork())){
                    perror("Fork Failed");
                    exit(1);
                }
                else if(kidpid == 0){                            //if child, execute this block -- handle in/out redirect (dup2) if their fd's aren't NULL
                    if(piper != 0){
                        if(-1 == (pipe(fildes))){
                            perror("Pipe failed on input fildes.\n");
                            exit(1);
                        }
                        if(-1 == (grandpid = fork())){
                            perror("Fork Failed");
                            exit(1);
                        }
                        else if(grandpid == 0){
                            if(rdIn)
                                dup_in(&rdIn);
                            if(dup2(fildes[1], 1) == -1){
                                perror("dup2 failed on output file for pipe.\n");
                                exit(1);
                            }
                            if(-1 == close(fildes[0])){
                                perror("close failed for pipe.\n");
                                exit(1);
                            }
                            if(-1 == close(fildes[1])){
                                perror("close failed for pipe.\n");
                                exit(1);
                            }
                            if(execvp(*argv1, argv1) < 0){                    //ready for execution, exit if it failed
                                perror("Execvp Failed");
                                exit(1);
                            }
                        }
                        else{                            //parent of grandchild
                            if(rdOut)
                                dup_out(&rdOut, &stderror);
                            //dup_in(&fildes[0]);
                            if(dup2(fildes[0], 0) == -1){
                                perror("dup2 failed on input file for pipe.\n");
                                exit(1);
                            }
                            if(-1 == close(fildes[0])){
                                perror("close failed for pipe.\n");
                                exit(1);
                            }
                            if(-1 == close(fildes[1])){
                                perror("close failed for pipe.\n");
                                exit(1);
                            }
                            if(execvp(*(argv1+piper), argv1+piper) < 0){                    //ready for execution, exit if it failed
                                perror("Execvp Failed");
                                exit(1);
                            }
                        }
                    }
                    else{
                        if(rdIn)
                            dup_in(&rdIn);
                        if(rdOut)                            //repeat for rdOut (std out == 1)
                            dup_out(&rdOut, &stderror);
                        if(execvp(*argv1, argv1) < 0){                    //ready for execution, exit if it failed
                            perror("Execvp Failed");
                            exit(1);
                        }
                    }
                }
                else {
                    if(!amps) while (wait(&status) != kidpid);                //if parent, wait for child to finish
                    else printf("%s [%d]\n", *argv1, kidpid);        //since child is backgrounded parent doesnt wait and instead prints child's pid and prog name
                }
            }
        }
        else if(parseCode < -2){             //parseCode < -2 means that there was in error
            if (parseCode == -3) parseCode = 3;
            else parseCode = 0;            //else parseCode == -4 which is a invalid progname error, and we dont have to worry about prompt reissuing twice (so dont set to 3)
            history++;
        }

    }
}
void dup_out(int *rdOut, int *stderror){
    if(dup2(*rdOut, 1) == -1){
        perror("dup2 failed on output file.\n");
        exit(1);
    }
    if(*stderror){                        //if flag set then dup2 (stderr == 2) as well
        if(dup2(*rdOut, 2) == -1){
            perror("dup2 failed on output file (stderr).\n");
            exit(1);
        }
    }
    if(-1 == close(*rdOut)){
        perror("close failed on output redirect file.\n");
        exit(1);
    }
}

void dup_in(int *rdIn){
    if(dup2(*rdIn, 0) == -1){                //(stdin == 0)
        perror("dup2 failed on input file.\n");
        exit(1);
    }
    if(-1 == close(*rdIn)){                    //close fd -- don't need anymore
        perror("close failed on input redirect file.\n");
        exit(1);
    }
}

int parse(char *argv1[], int *rdI, int *rdO, int *arg, int *stde, int *amp, int *script, int *hist, int *pip, char histaux[], char histfiles[]){
    int c;
    char s[STORAGE];
    int histindex = 0, fileindex = 0;
    if((*hist)<9){
        histindex = 25600*(*hist);        //if hist == 0 then index begins at 0 
        fileindex = 519*(*hist);
        int j = fileindex;
        while(j<fileindex+4)            //initialize as nulls, so if [0],[1],[2],[3] are not equal to null then I know there are some redirects, pipes, and ampersands to handle
            histfiles[j++] = '\0';
    }

    c = getword(s);                //get the word right off the bat so that a check for "!!" can be made immediately

    if(c == 2 && s[0] == '!' && (s[1] == '!' || isdigit(s[1]))){
        int command = 0;
        if((*hist)<9)
            strcpy(histaux+histindex, s);
        if(isdigit(s[1]))
            command = (s[1] - 38);        //i looked up ascii codes for digits, so this essentially converts the digit to its real value+10 in binary, reason for subtracting only 38 explained below
        while (c > 0)            //this is to pass over all the words that may follow "!!" since they get ignored
            c  = getword(s);
        if(command) return command;        //the reason for subtracting only 38 was to make the (unused) return values 10, 11, 12, 13, ... 18, 19 correspond to history 0, 1, 2, 3, ... 8, 9 respectively
        if(*arg){
            if((*hist)<9){
                char n = (*hist) + '0';            //conver the value of hist to char
                histaux[histindex+2] = n;
            }
            return 1;        // if arg != 0 return 1 so that '!!' executes the previous command
        }
        else return 0;            //else return 0 so that '!!' to notify that there was NO previous command
    }
    else if(s[0] == '!'){
        while(getword(s)>0);
        return 10;            //this code will cause an 'event not found'
    }
    int metaFlag = -1, echoflag = 0;    //metaFlag is set to 0, 1, 2, or 3 for when the parser sees a <, >, >&, & respectively. so that the next word is handled correctly
    static int ambigout = 0, ambin = 0;
    static int disp = 0;            //used to copy words on a static array w/o overlapping or spacing.
    *arg = *rdI = *rdO = *stde = *amp = *pip = 0;    //set these values to 0, since parse gets called for one line expecting new arguments/redirects
    static char line[MAXLINE];            
    if(c == -1 && (strcmp(s, "logout") == 0))
        return -1;
    if(c == 0 || c == -1) return 0;     //if this -1 happens then it is end of the input line, whereas previous means logout
    if(c== -2) return -1;            // getword returns a -2 for when it is finished processing input files, like "./p2 < input" (rather than a user typing in commands)
    while(c>0){
        if(c == 1 && s[0] == '#' && (*script) == 1){
            if(metaFlag != -1 && metaFlag !=3)
                perror("Metacharacter missing argument.");
            while(c > 0)
                c = getword(s);
            return 1;
        }
        if(metaFlag == 3){        //Last word was '&', with another word to be processed -- treat this as an argument! 
            *(argv1 + (*arg)) = "&";    
            (*arg)++;
            metaFlag = -1;
        }
        if (s[0] == '>' || s[0] == '<' || s[0] == '&' || s[0] == '|'){    
            if(metaFlag != -1){
                perror("Invalid use of metachars: consecutive use.");            //consecutive metachar strings
                return -3;
            }
            else{                                    //process metachars and set the flag to 0, 1, 2 or 3 (corresponding to <, >, >&, &)
                
                if(s[0] == '|' && (strlen(s) == 1)){
                    if (pipeFlag){
                        pipeFlag=0;    //decrement count of slashed out pipes
                        if(*arg == 100){
                            perror("Too many arguments.");
                            return -3;
                        }
                        strcpy(line+disp, s);
                        if((*hist)<9)
                            strcpy(histaux+(histindex+disp), s);
                        *(argv1 + (*arg)) = line+disp;
                        disp += c+1;
                        (*arg)++;
                    }
                    else metaFlag = 6;
                }
                else if(s[0] == '<' && (strlen(s) == 1))
                    metaFlag = 0;                //rdIn = 0
                else if(s[0] == '>' && c <= 3){
                    if(c == 1)
                        metaFlag =  1;            //rdOut = 1
                    else if(c == 2){
                        if(s[1] == '&') metaFlag = 2;        //'>&'
                        else if(s[1] == '>') metaFlag = 4;    //'>>'
                        else perror("Output redirect metachars used incorrectly.");
                    }
                    else if(c == 3 && s[2] == '&') 
                        metaFlag = 5;    //'>>&'
                    else perror("Output redirect metachars used incorrectly.");            //invalid use of rdOut
                }    
                else if(s[0] == '&'){
                    if(!(*arg)){                //if arg == 0 and the first word is & then assumed it's the program's name
                        strcpy(line+disp, s);
                        if((*hist)<9)
                            strcpy(histaux+(histindex+disp), s);
                        *(argv1) = line+disp;        //then increment arg count
                        disp += (strlen(s) + 1);    //to account for null terminator
                        (*arg)++;
                    }
                    else if(strlen(s) == 1)            //else set flag == 3
                        metaFlag = 3;
                }
                else{                        //if not any of above cases assume it is a arg    OR NOT, maybe just throw an error
                    strcpy(line+disp, s);
                    if((*hist)<9)
                        strcpy(histaux+(histindex+disp), s);
                    argv1[(*arg)++] = line+disp;
                    disp += (strlen(s) + 1);    
                }
            }
        }
        else if(metaFlag != -1){                //metaFlag has been set and the word that follows does NOT begin with a metachar, so assume this is rdIn/Out file and its NOT &
            if(metaFlag == 0){                //redirect stdin
                if(*rdI || ambin == 1){
                    if((*hist)<9)
                        histfiles[fileindex] = '9';                //'9' == ambiguous redirect
                    while((c=getword(s)) >0);
                    perror("Ambiguos input redirect");
                    ambin = 0;
                    return -3;
                }
                else if((*rdI = open(s, O_RDONLY)) == -1){        
                    ambin = 1;            //file nonexistent
                    metaFlag = -1;
                    if((*hist)<9){
                        histfiles[fileindex] = '1';            //'1' meaning, input redirect is true
                        strcpy(histfiles+(fileindex+4), s);
                    }
                }
                else if((*hist) < 9){
                    histfiles[fileindex] = '1';            //'1' meaning, input redirect is true
                    strcpy(histfiles+(fileindex+4), s);
                }
            }    
            else if(metaFlag == 1 || metaFlag == 2){
                if(metaFlag == 2) *stde = 1;                    //this is a flag to mark >&
                if(*rdO || ambigout == 1){
                    if((*hist)<9)
                        histfiles[fileindex+1] = '9';                //'9' == ambiguous redirect
                    while((c=getword(s)) >0);
                    perror("Ambiguos output redirect");    //if(*rdO) then user already entered a "> 'outfile'" making for an ambiguous output redirect
                    ambigout = 0;
                    return -3; 
                }
                else if((*rdO = open(s, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1){
                    if((*hist)<9){
                        strcpy(histfiles+(fileindex+260), s);
                        if(metaFlag == 1)
                            histfiles[fileindex+1] = '1';        // == '>'
                        else histfiles[fileindex+1] = '2';        // == '>&'
                    }
                    ambigout = 1;
                    metaFlag = -1;
                }
                else if((*hist)<9){
                    strcpy(histfiles+(fileindex+260), s);
                    if(metaFlag == 1)
                        histfiles[fileindex+1] = '1';        // == '>'
                    else histfiles[fileindex+1] = '2';        // == '>&'
                }
            }
            else if(metaFlag == 4 || metaFlag == 5){
                if(metaFlag == 5) *stde = 1;                    //this is a flag to mark >&
                if(*rdO || ambigout == 1){
                    if((*hist)<9)
                        histfiles[fileindex+1] = '9';                //'9' == ambiguous redirect
                    while((c=getword(s)) >0);
                    perror("Ambiguos output redirect");            //if(*rdO) then user already redirected output once, thus making it ambiguous
                    ambigout = 0;
                    return -3;
                }
                else if((*rdO = open(s, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR)) == -1){
                    if((*hist)<9){
                        strcpy(histfiles+(fileindex+260), s);
                        if(metaFlag == 4)
                            histfiles[fileindex+1] = '4';        // == '>>'
                        else histfiles[fileindex+1] = '5';        // == '>>&'
                    }
                    ambigout = 1;
                    metaFlag = -1;
                }
                else if((*hist)<9){
                    strcpy(histfiles+(fileindex+260), s);
                    if(metaFlag == 4)
                        histfiles[fileindex+1] = '4';        // == '>>'
                    else histfiles[fileindex+1] = '5';        // == '>>&'
                }
            }
            else if(metaFlag == 6){
                if((*pip) != 0){
                    while((c=getword(s)) >0);
                    if((*hist)<9)
                        histfiles[fileindex+3] = '9';                //'9' == illegal piping 
                    perror("Illegal use of the pipe.");
                    return -3;
                }
                argv1[(*arg)++] = '\0';            //mark end of list with null
                (*pip) = (*arg);            //int *pip will mark where in the newargv1 array the beginning of the new program and args that follow the pipe
                if(s[0] == '-'){             //error code: progname cant start with '-'
                    perror("Error: Program name may not begin with a '-' character.");
                    while(c > 0) c = getword(s);    //ignore the following words after invalid prog name
                    return -4;            //returning -4 instead of -3 distinguishes an invalid prog name error from a redirect error
                }
                char str[2];
                if((*hist)<9){
                    strcpy(histaux+(histindex+disp), s);
                    histfiles[fileindex+3] = '1';        //pipe flag 
                    char str[2];
                    sprintf(str, "%d\0", (*pip));        //copy location of program in this array, could be two digits
                    strcpy(histfiles+(fileindex+516), str);
                }
                strcpy(line+disp, s);
                argv1[(*arg)++] = line+disp;                        //then assign the address of the 1st char of the null-term'd string to argv[arg++]
                disp += (strlen(s) + 1);    
            }
            metaFlag = -1;                //no need to handle metFlag == 3, b/c that case gets handled right at the start of this while loop. now reset the flag after out/in rds
        }
        else{                            //else either prog or args or flags
            if(!(*arg)){                    //if true then this word is assumed to be the program's name, or the program name following pipe
                if(s[0] == '-'){             //error code: progname cant start with '-'
                    perror("Error: Program name may not begin with a '-' character.");
                    while(c > 0) c = getword(s);    //ignore the following words after invalid prog name
                    return -4;            //returning -4 instead of -3 distinguishes an invalid prog name error from a redirect error
                }
                if((strcmp(s, "echo")) == 0 && (strlen(s) == 4)) echoflag = 1;        //if prog is echo then set this flag, b/c it causes '&' to be treated differently
                strcpy(line+disp, s);                            //copy to line buffer
                if((*hist)<9)
                    strcpy(histaux+(histindex+disp), s);
                argv1[(*arg)++] = line+disp;                        //then assign the address of the 1st char of the null-term'd string to argv[arg++]
                disp += (strlen(s) + 1);    
            }
            else{                    //either flags or args
                if(*arg == 100){
                    perror("Too many arguments.");            
                    return -3;
                }
                strcpy(line+disp, s);
                if((*hist)<9)
                    strcpy(histaux+(histindex+disp), s);
                *(argv1 + (*arg)) = line+disp;        //else its an arg
                disp += (strlen(s) + 1);
                (*arg)++;
            }
        }
        c = getword(s);
        if(strcmp(s, "logout") == 0)                //if user wants to logout then it must be the first word. else it gets treated like an arg
            c = 6;
    }    
    if(metaFlag == 3){
        *amp = 1;
        if((*hist)<9)
            histfiles[fileindex+2] = '1';            //amps == '1' equals true
    }
    if(metaFlag != -1 && metaFlag != 3){
        if(metaFlag == 0){
            if((*hist)<9)
                histfiles[fileindex] = '8';            // == 8 means missing argument for the metacharacter
            perror("Missing argument for input redirect metacharacter.");
        }
        else if(metaFlag <= 5){                        //covering output redirect metachars >, >>, >&, >>&
            if((*hist)<9)
                histfiles[fileindex+1] = '8';            // == 8 means missing argument for the metacharacter
            perror("Missing argument for output redirect metacharacter.");
        }        
        else{                                //else its 6, a pipe
            if((*hist)<9)
                histfiles[fileindex+3] = '8';            // == 8 means missing argument for the metacharacter
            perror("Missing argument for pipe metacharacter.");
        }
    }
    argv1[(*arg)] = '\0';            //mark end of list with null
    if((*hist)<9)
        histaux[(histindex+disp)] = '\0';
    disp = 0;
    if(ambigout == 1 || ambin == 1){        //this chunk handles output redirect errors
        if(ambigout == 1){
            ambigout = 0;
            if(histfiles[fileindex+1] == '4' || histfiles[fileindex+1] == '5')
                perror("Error, file doesn't exist for append");
            else perror("Error, file already exists for output redirect");
        }
        if(ambin == 1){
            ambin = 0;
            perror("Error, file doesn't exist for input redirect");
        }
        return -3;
    }
    if(metaFlag != -1 && metaFlag != 3) return -3;
    return 1;             //returns 1 upond successful parse
}



//code for sighandler -- modeled off of ~cs570/sighandler.c
void sighandler(int signum){
}

int specialparse(char *argv1[], int *rdI, int *rdO, int *arg, int *stde, int *amp, int number, int *pip, char histaux[], char histfiles[]){
    int displacement = 0, j = 0, index = 0, fileindex = 0;    
    (*rdO) = (*rdI) = 0;
    index = 25600*number;        //if hist == 0 then index begins at 0 
    while(histaux[index] == '!'){
        if(histaux[index+1] == '!'){
            number = (histaux[index+2] -48)-1;    
            index = 25600*number;
        }
        else if(isdigit(histaux[index+1])){
            number = (histaux[index+1] - 48)-1;
            index = 25600*number;
        }
    }
    fileindex = 519*(number);    //calculate the correct displacements for the correct history index
    if(histfiles[fileindex+3] == '9'){                //these if statements check for potential errors that occured on the corresponding command
        perror("Illegal use of the pipe metcharacter.");
        return 3;
    }
    if(histfiles[fileindex+3] == '8'){
        perror("Missing arguemtn for pipe metachar.");
        return 3;
    }
    if(histfiles[fileindex] == '9' || histfiles[fileindex+1] == '9'){
        perror("Ambiguous Redirect");
        return 3;
    }
    if(histfiles[fileindex] == '8'){
        perror("Missing argument for input metachar.");
        return 3;
    }
    if(histfiles[fileindex+1] == '8'){
        perror("Missing argument for output metachar.");
        return 3;
    }
    if(histfiles[fileindex+3] == '1')    
        *pip = atoi(histfiles+(fileindex+516));            //starting at this location in histfiles is a string in digits representing the location of where the first word following the pipe lives, convert it to int
    
    while(histaux[index+displacement] != '\0'){
        argv1[j++] = histaux+(index+displacement);
        displacement += 1 + (strlen(histaux+(index+displacement)));
        if(j+1 == *pip)                        //either break when its finished parsing the command and args, OR when it reaches the pipe
            break;
    }
    argv1[j++] = '\0';                    //null terminate the command and its args
    if(histfiles[fileindex+3] == '1'){            //if j < than the arg count then assume there was a pipe command and process the command following pipe and its args
        while(histaux[index+displacement] != '\0'){
            argv1[j++] = histaux+(index+displacement);
            displacement += 1 + (strlen(histaux+(index+displacement)));
        }
        argv1[j] = '\0';                //null term the second command and its args
    }
    if(histfiles[fileindex] == '1'){            //'1' corresponds to "there was an input redirect for this command"
        if((*rdI = open(histfiles+(fileindex+4), O_RDONLY)) == -1){
            perror("Input file does not exist.");            //error: non existent file
            return 3;
        }
    }
    if(histfiles[fileindex+1] != '\0'){                    //if not null then there corresponds a code that either means >, >&, >>, >>&
        int code = histfiles[fileindex+1] - 48;
        if(code <= 2){
            if((*rdO = open(histfiles+(fileindex+260), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1){
                perror("File already exists. Cannot overwrite.");            
                return 3;
            }
            if(code == 2) *stde =1;            //if '>&'
        }
        else{
            if((*rdO = open(histfiles+(fileindex+260), O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR)) == -1){
                perror("File does not exist. Can't append.");            
                return 3;
            }
            if (code == 5) *stde = 1;        //if >>&
        }
    }
    if(histfiles[fileindex+2] == '&') *amp = 1;
    return 1;
} 
