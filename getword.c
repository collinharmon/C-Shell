/*
Collin Harmon
*/

#include "getword.h"
#define BUFFER 25600        //255 (max chars for a word) * 100 (max words per line) = 25500 +100 for null terms

extern int pipeFlag;

int getword(char *w){
static int wordsize = 0, tokens = 0, numruns = 0, displacement = 0;
int ch;
static char aux[BUFFER];                //buffer array where tokenization happens
int wasDelim =1, wasSlash = 0, metaCount = 0, index = 0;    //wasDelim and wasSlash act as boolean vars, metaCount tracks consecutive metachars
    if(!tokens){
        while ( (ch = getchar()) != EOF && index+1 < BUFFER) {        //reasoning for adding one to index is so that last spot can be reserved for null terminator if need be
            if(wordsize+1 == STORAGE){            //if max char limit for word reached then tokenize 
                aux[index++] = '\0'; 
                tokens++;
                wasDelim = 1;
                wordsize = 0;
            }
            if(metaCount == 2 && ch != '&'){    //if this is true then << is tokenized 
                metaCount = 0;
                aux[index++] = '\0';
                tokens++;
                wasDelim = 1;
                wordsize = 0;
            }
            if(isspace(ch) && ch != '\n' && ch != '\t'){
                if((!wasDelim) && (!wasSlash)){        //tokenize word when space follows a non whitespace
                    tokens++;
                    metaCount = 0;
                    aux[index++] = '\0';    
                    wasDelim = 1;    
                    wordsize = 0;
                } 
                else if (wasSlash){            //if space follows a slash then count it
                    wasDelim = 0;
                    aux[index++] = ch;    
                    wordsize++;
                    wasSlash = 0;
                }    
                else wordsize++;            //else ignore it
            }
            else if(ch == '\n'){
                if(!wasDelim){                //if newline found after token chars then tokenize
                    wasDelim = 1;
                    aux[index++] = '\0';
                    tokens++;    
                    wordsize = 0;
                }
                aux[index++] = ch;            //treat newline as a token since it returns "0 and [] "
                aux[index++] = '\0';
                tokens++;
                wordsize = 0;    
                wasSlash = 0;
                break;
            }
            else if(ch == '\\') {
                if(wasSlash){            //if slash preceeded a slash then count slash as normal char
                    aux[index++] = ch;
                    wordsize++;
                    wasSlash = 0;
                    wasDelim = 0;
                }    
                else{
                    wasSlash = 1;    //maybe ++ size, decided to not let a slash count towards wordsize
                    if (metaCount){        //if a slash follows a metachar then we want to tokenize the metachar
                        metaCount = 0;
                        tokens++;
                        wordsize = 0;
                        aux[index++] = '\0'; 
                    }
                }
            }
            else if (ch == '<' || ch == '>' || ch == '|' || ch == '&'){
                if(wasSlash){                    //if metaChar follows a slash then treat it normally
                    if (ch == '|') pipeFlag = 1;        //might need two flags, one counting the amount of \|, and another locating a possible pipe not slashed out
                    wasDelim = 0; 
                    wordsize++;
                    aux[index++] = ch;    
                    wasSlash = 0;
                }
                else{
                    if (metaCount == 0){
                        if(!wasDelim){            //if metachar follows a normal char then tokenize the previous word
                            aux[index++] = '\0'; 
                            tokens++;    
                            wordsize = 0;
                            wasDelim = 1;    
                        }
                        if(ch != '>'){            //the only multi-char metachar tokens begin with '>' so if not that
                            aux[index++] = ch;    //then tokenize immediately
                            aux[index++] = '\0';
                            tokens++;
                            wordsize = 0;
                        }    
                        else{                //begins with '>' and may be multichar token
                            aux[index++] = ch;
                            wordsize++;
                            metaCount++;
                            wasDelim = 0;
                        }
                    }
                    else if (metaCount == 1){
                        if(ch == '&'){            //then token is ">&"
                            metaCount = 0;
                            aux[index++] = ch;
                            aux[index++] = '\0';
                            tokens++;
                            wordsize = 0;
                            wasDelim = 1;
                        }
                        else if(ch == '>'){
                            metaCount++;        //now metacount is = to two with '>>' 
                            aux[index++] = ch;
                            wordsize++;
                        }
                        else{                //if the initial '>' is followed by a metachar that is NOT 
                            metaCount = 0;        //a '&' or '>' then we have two tokens
                            wordsize = 0;
                            aux[index++] = '\0';    //null term first token
                            tokens+=2;        //add two to tokens (see comment above)
                            aux[index++] = ch;
                            aux[index++] = '\0';
                            wasDelim = 1;
                        }
                    }
                        else{            //metaCount == 2 here, and we know ch == to '&' cause of first if test 
                            metaCount = 0;    // at the beginning of the while loop, resets count to zero if not ampersand
                            aux[index++] = ch;
                            aux[index++] = '\0';
                            wordsize = 0;
                            tokens++;
                            wasDelim = 1; 
                        }
                            
                } 
            
            }        
            else{                //else regular char
                if(metaCount){        //this says if ch = regular char and it follows metachars then tokenize the metachars
                    metaCount = 0;
                    tokens++;
                    wordsize = 0;
                    aux[index++] = '\0'; 
                }
                wordsize++;
                aux[index++] = ch;
                wasDelim = 0;
                wasSlash = 0;
            }
        }
    }
    if(!wasDelim){            //handles premature EOF
        wasDelim = 1;
        aux[index++] = '\0'; 
        tokens++;
        wordsize = 0;
    }
    if(numruns == tokens)        //if test which checks if all the tokens have been processed 
        return -2;
    else{                //else if more tokens need to be processed copy from aux to w, keeping track of displacement
        strcpy(w, aux+displacement);    //add one for the null char
        if((strcmp(w, "logout")) == 0 && strlen(w) == 6){
            displacement += strlen(aux+displacement) + 1;        //adjustment feb 23
            return -1;
        }
        numruns++;
        //ill need to check if line is terminated by eof then reset vars
        displacement += strlen(aux+displacement) + 1;
        //if(w[0] == '\n' && numruns == tokens){            //adjustment as of 22 -- this line is last input to getword
        if(w[0] == '\n'){            //adjustment as of 22 -- this line is last input to getword
            w[0] = '\0';
            tokens = 0;            //addjustment feb 22 --- reset all static vars
            numruns = 0;
            index = 0;
            wordsize = 0;
            displacement = 0;
        }
        //else w[0] = '\0';
        return strlen(w);
    }
}
