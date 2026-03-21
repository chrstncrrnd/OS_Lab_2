#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
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


// process structure (used for managing processes)
typedef struct {
  pid_t pid;
  int argc;
  // name is argv[0]
  char argv[max_args][max_line];
  int fd_in;
  int fd_out;
} proc_t;


// vector of processes
typedef struct {
  proc_t values[max_redirections];
  // number of elements
  size_t size;
} vec_proc;


void print_vec_proc(const vec_proc* vector){
  if(vector == NULL){
    printf("NULL\n");
    return;
  }

  for (size_t i = 0; i < vector->size; i++){
    printf("Program: %s\n", vector->values[i].argv[0]);
    for (size_t j = 1; j < (size_t)vector->values[i].argc; j++){
      printf("Argument: %s\n", vector->values[i].argv[j]);
    }
  }
}

// once we have parsed the line into proc_t, we can proceed to execute them
void exec_line(vec_proc* line){
  print_vec_proc(line);
}


// line to vector of processes
bool process_line(char* line, int line_number, vec_proc* out){
  strip(line);

  size_t line_len = strlen(line);
  if (line_len == 0){
    return false;
  }


  if (line_number == 0){
    if (strcmp(line, "## Uc3mshell P2") != 0){
      fprintf(stderr, "ERROR: Unexpected first line, got %s!\n", line);
      _exit(-1);
    }else{
      return false;
    }
  }
  if (line[0] == '#'){
    return false;
  }


  out->size = 0;
  char* word;
  proc_t current = {
    .argc = 0
  };

  int pipe_fd[2] = {0, 0};
  int prev_pipes_stdout = 0;


  while ((word = strtok(current.argc == 0 ? line : NULL, " ")) != NULL){
    if (strcmp(word, "|") == 0){
      if(prev_pipes_stdout){
        current.fd_out = pipe_fd[1];
      }
      prev_pipes_stdout = 1;
      //pipe(pipe_fd);
      current.fd_out = pipe_fd[0];
      out->values[out->size++] = current;
      if (out->size > max_redirections){
        fprintf(stderr, "Too many redirections on line: %d", line_number);
        return false;
      }
      current.argc = 0;
    }else{
      strcpy(current.argv[current.argc++], word);
    }

  }

  if (current.argc != 0 && out->size < 3){
    out->values[out->size++] = current;
  }
  return true;
}


void process_file(char* filename){
  int fd = open(filename, O_RDONLY);
  if (fd < 0){
    perror("ERROR: Couldn't open input file");
    _exit(0);
  }

  char f_buf[F_RD_BUF_SIZE], line[max_line] = {0};
  int line_number = 0;
  ssize_t bytes_read, offset;
  vec_proc to_exec = {
    .size = 0
  };
  bool success;



  // read in chunks
  while ((bytes_read = read(fd, f_buf, F_RD_BUF_SIZE)) > 0){
    offset = -(ssize_t)strlen(line);
    for (ssize_t i = 0; i < bytes_read; i ++){
      if (f_buf[i] == '\n'){
        line[i - offset] = '\0';
        // vector is freed once executed
        to_exec.size = 0;
        success = process_line(line, line_number++, &to_exec);
        if (success){
          exec_line(&to_exec);
        }
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

