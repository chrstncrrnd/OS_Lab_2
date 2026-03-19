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
  char** argv;
} proc_t;


// vector of processes
typedef struct {
  proc_t* values;
  // number of elements
  size_t size;
  // amount of memory allocated
  size_t capacity;
} vec_proc;

vec_proc* create_vec_proc(){
  vec_proc* out = malloc(sizeof(vec_proc));
  if (out == NULL){
    perror("ERROR: allocating memory for process vector struct!");
    free(out);
    _exit(-1);
  }
  // initial capacity of 8 is reasonable
  out->capacity = 8;
  out->size = 0;
  out->values = malloc(sizeof(proc_t) * out->capacity);

  // check that the memory was correctly allocated
  if (out->values == NULL){
    perror("ERROR: allocating memory for process vector elements!");
    _exit(-1);
  }
  return out;
}

// append value to vector
void append_vec_proc(vec_proc* vector, proc_t value){
  if (vector->size == vector->capacity){
    // reallocate vector
    vector->capacity = vector->capacity * 2;
    vector->values = realloc(vector->values, vector->capacity * sizeof(proc_t));
    if(vector->values == NULL){
      perror("ERROR: reallocating memory for process vectors!");
      _exit(-1);
    }
  }
  vector->values[vector->size++] = value;
}

// Don't forget to set pointer equal to null afterwards!
void destroy_vec_proc(vec_proc* to_destroy){
  if (to_destroy == NULL){
    return;
  }
  free(to_destroy->values);
  free(to_destroy);
}


// once we have parsed the line into proc_t, we can proceed to execute them
void exec_line(vec_proc* line){
  line = (void*) line;
}


// line to vector of processes
vec_proc* process_line(char* line, int line_number){ 
  strip(line);

  size_t line_len = strlen(line);
  if (line_len == 0){
    return NULL;
  }


  if (line_number == 0){
    if (strcmp(line, "## Uc3mshell P2") != 0){
      fprintf(stderr, "ERROR: Unexpected first line, got %s!\n", line);
      _exit(-1);
    }else{
      return NULL;
    }
  }
  if (line[0] == '#'){
    return NULL;
  }


  vec_proc* out = create_vec_proc();
  char* word;
  int words_read = 0;
  proc_t current;
  while ((word = strtok(words_read > 0 ? NULL : line, " ")) != NULL){
    words_read ++;
  }
  return out;
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
  // read in chunks
  while ((bytes_read = read(fd, f_buf, F_RD_BUF_SIZE)) > 0){
    offset = -(ssize_t)strlen(line);
    for (ssize_t i = 0; i < bytes_read; i ++){
      if (f_buf[i] == '\n'){
        line[i - offset] = '\0';
        // vector is freed once executed
        vec_proc *to_exec = process_line(line, line_number++);
        if (to_exec != NULL){
          exec_line(to_exec);
          destroy_vec_proc(to_exec);
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

