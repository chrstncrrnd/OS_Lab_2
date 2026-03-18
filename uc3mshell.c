#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "strstrp.h"

const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15

#define true 1
#define false 0
#define bool int


void process_line(char* line){
  size_t line_len = strlen(line);
  if (line_len == 0)
    return;

}


void process_input(char* input){
  input = (void*) input;
  //...
  // for l in input.lines() process_line(l)
  //...
}

void print_usage(char* bin_name){
  printf("Usage: %s <input_file>\n", bin_name);
}


int main(int argc, char *argv[]) {
  char a[20] = "";
  printf("Before: |%s|\n", a);
  strip_right(a);
  printf("After right: |%s|\n", a);
  strip_left(a);
  printf("After: |%s|\n", a);
  return 0;
  if (argc != 2){
    print_usage(argv[0]);
    return -1;
  }
}

