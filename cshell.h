/*
written by collin harmon
*/

#include "getword.h"
#define MAXITEM 100
#define MAXLINE 25600            //max char count = 255 + 1 for null terminator, with 100 items comes to 25600
#define MAXHISTORY 230400        //multiply previous by 9 (max history)

int cshell(int argc, char *argv[]);

int parse(char *argv1[], int *rdI, int *rdO, int *arg, int *stde, int *amp, int *script, int *hist, int *pip, char histaux[], char histfiles[]);

int execute(char *argv1[], int *arg, int *amp);
void sighandler(int signum);
int specialparse(char *argv1[], int *rdI, int *rdO, int *arg, int *stde, int *amp, int number, int *pip, char histaux[], char histfiles[]);
