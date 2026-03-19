#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "str_strip.h"

const int max_line = 1024;
const int max_commands = 10;
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15

#define BUF_SIZE 1024

#define true 1
#define false 0
#define bool int


void process_line(char* line, int line_number){
  size_t line_len = strlen(line);
  strip(line);
  if (line_number == 0){
    if (strcmp(line, "## Uc3mshell P2")){
      fprintf(stderr, "ERROR: Unexpected first line!\n");
      _exit(-1);
    }
  }
  if (line_len == 0)
    return;

  printf("Line: |%s|\n", line);
}


void process_file(char* filename){
  int fd = open(filename, O_RDONLY);
  if (fd < 0){
    fprintf(stderr, "ERROR: Couldn't open input file: %s.\n", filename);
    _exit(0);
  }

  char* line;
  char buf[BUF_SIZE];
  int line_number = 0, bytes_read;
  while ((bytes_read = read(fd, buf, BUF_SIZE))){
    while((line = strtok(line_number > 0 ? NULL : buf, "\n"))){
      process_line(line, line_number++);
    }
  }
}

void print_usage(char* bin_name){
  printf("Usage: %s <input_file>\n", bin_name);
}


int main(int argc, char *argv[]) {
  if (argc != 2){
    print_usage(argv[0]);
    return -1;
  }
  process_file(argv[1]);
}

