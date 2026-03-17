#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15


void process_line(char* line){
  size_t line_len = strlen(line);
  if (line_len == 0)
    return;

}


void process_input(char* input){
  //...
  // for l in input.lines() process_line(l)
  //...
}


int main(int argc, char *argv[]) {

}

