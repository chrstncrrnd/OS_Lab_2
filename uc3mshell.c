#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "str_strip.h"

#define max_commands 10
#define max_redirections 3 // stdin, stdout, stderr
#define max_args 15
#define max_line 1024


// File Read Buffer size. Must be at least max_line
#define F_RD_BUF_SIZE 1024
#define true 1
#define false 0
#define bool int


void process_line(char* line, int line_number){ 
  strip(line);

  size_t line_len = strlen(line);
  if (line_len == 0){
    return;
  }


  if (line_number == 0){
    if (strcmp(line, "## Uc3mshell P2") != 0){
      fprintf(stderr, "ERROR: Unexpected first line, got %s!\n", line);
      _exit(-1);
    }
  }

  printf("Line: |%s|\n", line);
}


void process_file(char* filename){
  int fd = open(filename, O_RDONLY);
  if (fd < 0){
    fprintf(stderr, "ERROR: Couldn't open input file: %s.\n", filename);
    _exit(0);
  }

  char f_buf[F_RD_BUF_SIZE], line[max_line] = {0};
  int line_number = 0;
  ssize_t bytes_read, offset;

  while ((bytes_read = read(fd, f_buf, F_RD_BUF_SIZE)) > 0){
    offset = -(ssize_t)strlen(line);
    for (ssize_t i = 0; i < bytes_read; i ++){
      if (f_buf[i] == '\n'){
        line[i - offset] = '\0';
        process_line(line, line_number++);
        line[0] = '\0';
        offset = i + 1;
        continue;
      }
      line[i - offset] = f_buf[i];
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

