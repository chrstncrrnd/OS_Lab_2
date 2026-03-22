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
#define token_max_len 256
// File Read Buffer size. Must be at least max_line
#define F_RD_BUF_SIZE 1024
#define true 1
#define false 0
#define bool int


// command struct (used for managing processes)
typedef struct {
  pid_t pid;
  int argc;
  // name is argv[0]
  char argv[max_args][max_line / max_args]; // TODO: THIS NEEDS OPTIMIZING
  int in;
  int out;
  int outerr;
  bool bg;
  char filev[40][3];
} cmd_t;

void init_cmd_t(cmd_t* val){
  val->pid = -1;
  val->argc = 0;
  val->in = STDIN_FILENO;
  val->out = STDOUT_FILENO;
  val->outerr = STDERR_FILENO;
  val->bg = 0;
  val->filev[0][0] = '\0';
  val->filev[1][0] = '\0';
  val->filev[2][0] = '\0';
}

// dynamically sized vector of commands
typedef struct {
  cmd_t *values;
  // number of elements
  size_t size;
  // capacity of array
  size_t capacity;
} vec_cmd;



// create the vector of commands
vec_cmd* create_vec_cmd(){
  vec_cmd* out = malloc(sizeof(vec_cmd));
  if (out == NULL){
    perror("ERROR: allocating memory for process vector struct!");
    free(out);
    _exit(-1);
  }
  // initial capacity of 4 is reasonable (won't need to reallocate usually)
  out->capacity = 4;
  out->size = 0;
  out->values = malloc(sizeof(cmd_t) * out->capacity);

  // check that the memory was correctly allocated
  if (out->values == NULL){
    perror("ERROR: allocating memory for process vector elements!");
    _exit(-1);
  }
  return out;
}



// append value to vector
void append_vec_cmd(vec_cmd* vector, cmd_t value){
  if (vector->size == vector->capacity){
    // reallocate vector
    vector->capacity = vector->capacity * 2;
    vector->values = realloc(vector->values, vector->capacity * sizeof(cmd_t));
    if(vector->values == NULL){
      perror("ERROR: reallocating memory for process vectors!");
      _exit(-1);
    }
  }
  vector->values[vector->size++] = value;
}



// Don't forget to set pointer equal to null afterwards!
void destroy_vec_cmd(vec_cmd* to_destroy){
  if (to_destroy == NULL){
    return;
  }
  free(to_destroy->values);
  free(to_destroy);
}


void print_cmd(const cmd_t * command){
  printf("------------------------\n");
  printf("Program: %s\n", command->argv[0]);
  for (size_t j = 1; j < (size_t)command->argc; j++){
    printf("\tArgument: %s\n", command->argv[j]);
  }
  printf("File Descriptors:\n");
  printf("\tin: %d\n\tout: %d\n\touterr: %d\n\n", command->in, command->out, command->outerr);
}

void print_vec_cmd(const vec_cmd* vector){
  if(vector == NULL){
    printf("NULL\n");
    return;
  }

  for (size_t i = 0; i < vector->size; i++){
    print_cmd(&vector->values[i]);
  }
}


typedef enum{
  LS_INITIAL,
  LS_BUILDING_WORD_QUOTES,
  LS_BUILDING_WORD,
  LS_BUILDING_ERR_REDIR
} LexerState;


typedef enum{
  TOKEN_WORD,
  TOKEN_PIPE,
  TOKEN_REDIR_IN,
  TOKEN_REDIR_OUT,
  TOKEN_REDIR_ERR,
  TOKEN_NULL, // '\0'
  TOKEN_ERROR
} TokenType;


typedef struct {
  TokenType token;
  char lexeme[token_max_len];
} Token;


bool is_nonspace(char c){
  return c != ' ';
}

bool is_nonreserved(char c){
  return c != '>' && c != '<' && c != '!' && c != '|' && c != '"';
}

// finite automata for the lexer
Token get_next_token(char** input){

  char c;

  char buffer[token_max_len] = {0}; // TODO: this needs optimization
  size_t current_token_len = 0;
  LexerState state = LS_INITIAL;


  while((c = (*input)[0]) != '\0'){
    switch(state){
      case LS_INITIAL:
        if(c == ' '){
          (*input) ++;
          continue;
        }
        else if(c == '|'){
          (*input) ++;
          return (Token){
            .token = TOKEN_PIPE,
            .lexeme = ""
          };
        }
        else if(c == '>'){
          (*input) ++;
          return (Token){
            .token = TOKEN_REDIR_OUT,
            .lexeme = ""
          };
        }
        else if(c == '<'){
          (*input) ++;
          return (Token){
            .token = TOKEN_REDIR_IN,
            .lexeme = ""
          };
        }
        else if(c == '!'){
          (*input) ++;
          state = LS_BUILDING_ERR_REDIR;
          continue;
        }
        else if(is_nonspace(c) && is_nonreserved(c)){
          state = LS_BUILDING_WORD;
          if (current_token_len >= token_max_len){
            fprintf(stderr, "Error processing line: token too long!");
            return (Token){
              .token = TOKEN_ERROR,
              .lexeme = ""
            };
          }
          buffer[current_token_len++] = c;
          (*input) ++;
          continue;
        }
        else if(c == '"'){
          (*input)++;
          state = LS_BUILDING_WORD_QUOTES;
          continue;
        }
        // let caller handle syntax error!
        return (Token){
          .token = TOKEN_ERROR,
          .lexeme = ""
        };
        break;
      case LS_BUILDING_ERR_REDIR:
        if (c == '>'){
          (*input)++;
          return (Token){
            .token = TOKEN_REDIR_ERR,
            .lexeme = ""
          };
        }
        return (Token){
          .token = TOKEN_ERROR,
          .lexeme = ""
        };
        break;
      case LS_BUILDING_WORD:
        if(is_nonreserved(c) && is_nonspace(c)){
          if (current_token_len >= token_max_len){
            fprintf(stderr, "Error processing line: token too long!");
            return (Token){
              .token = TOKEN_ERROR,
              .lexeme = ""
            };
          }
          buffer[current_token_len++] = c;
          (*input) ++;
          continue;
        }else if(c == ' ' || c == '\0' || (is_nonreserved(c) == 0)){
          Token out = {
            .token = TOKEN_WORD,
            .lexeme = ""
          };
          // make sure that we don't overflow buffer to append null terminator
          if(current_token_len >= token_max_len){
            fprintf(stderr, "Error processing line: token too long!");
            return (Token){
              .token = TOKEN_ERROR,
              .lexeme = ""
            };
          }
          // put our buffer into the token struct
          buffer[current_token_len ++] = '\0';
          strcpy(out.lexeme, buffer);
          return out;
        }
        return (Token){
          .token = TOKEN_ERROR,
          .lexeme = "",
        };
        break;
      case LS_BUILDING_WORD_QUOTES:
        if(c == '"'){
          Token out = {
            .token = TOKEN_WORD,
            .lexeme = ""
          };
          // make sure that we don't overflow buffer to append null terminator
          if(current_token_len >= token_max_len){
            fprintf(stderr, "Error processing line: token too long!");
            return (Token){
              .token = TOKEN_ERROR,
              .lexeme = ""
            };
          }
          // put our buffer into the token struct
          buffer[current_token_len ++] = '\0';
          (*input) ++;
          strcpy(out.lexeme, buffer);
          return out;
        }else if(c == '\0'){
          // we opened some quotes but never closed them!
          return (Token){
            .token = TOKEN_ERROR,
            .lexeme = ""
          };
        }else {
          // make sure that we don't overflow buffer to append new character
          if(current_token_len >= token_max_len){
            fprintf(stderr, "Error processing line: token too long!");
            return (Token){
              .token = TOKEN_ERROR,
              .lexeme = ""
            };
          }
          // put our buffer into the token struct
          buffer[current_token_len ++] = c;
          (*input) ++;
        }
        break;
      default:
        printf("AAAAAA");
        return (Token){
          .token = TOKEN_ERROR,
          .lexeme = ""
        };
    }
  }
  return (Token){
    .token = TOKEN_NULL,
    .lexeme = ""
  };
}


// once we have parsed the line into cmd_t, we can proceed to execute them
void exec_line(vec_cmd* parsed_line){
  print_vec_cmd(parsed_line);
  return;
  pid_t pid;
  pid = fork();
  cmd_t *line = &parsed_line->values[0];
  // case child
  if (pid == 0){
    char* exec_args[16];
    for (int i = 0; i < line->argc; i++){
      exec_args[i] = line->argv[i];
    }
    exec_args[line->argc] = NULL;
    execvp(exec_args[0], exec_args);
    perror("Couldn't execute command correctly!");
  }
  // case parent
  else{
    wait(NULL);
    printf("Finished waiting for child!\n");
  }
}




// line to vector of processes
vec_cmd* parse_line(char* line, int line_number){
  strip(line);

  size_t line_len = strlen(line);
  if (line_len == 0){
    return NULL;
  }


  if (line_number == 0){
    if (strcmp(line, "## Uc3mshell P2") != 0){
      perror("ERROR: Unexpected first line!\n");
      _exit(-1);
    }else{
      return NULL;
    }
  }
  if (line[0] == '#'){
    return NULL;
  }
  char *store = line;
  Token latest_token;
  printf("-------------Line %d-------------\n", line_number);
  while((latest_token = get_next_token(&store)).token != TOKEN_NULL){
    printf("Token: \n\t");
    switch(latest_token.token){
      case TOKEN_WORD:
        printf("TOKEN_WORD");
        break;
      case TOKEN_ERROR:
        printf("TOKEN_ERROR");
        break;
      case TOKEN_REDIR_ERR:
        printf("TOKEN_REDIR_ERR");
        break;
      case TOKEN_REDIR_IN:
        printf("TOKEN_REDIR_IN");
        break;
      case TOKEN_REDIR_OUT:
        printf("TOKEN_REDIR_OUT");
        break;
      case TOKEN_PIPE:
        printf("TOKEN_PIPE");
        break;
      case TOKEN_NULL:
        printf("TOKEN_NULL");
    }
    printf("\nLexeme:\n\t");
    printf("%s\n", latest_token.lexeme);
  }
  return NULL;
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
        vec_cmd* parsed_line = parse_line(line, line_number++);
        if (parsed_line != NULL){
          exec_line(parsed_line);
          destroy_vec_cmd(parsed_line);
          parsed_line = NULL;
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

