#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "str_strip.h"

// TODO: eliminar todo de abajo y este
// TODO: eliminar TODOs
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


// Really doesn't need to be a macro but i wanted to mess around
// with macros
#define ParserSyntaxError(line_number, out) { \
  fprintf(stderr, "Syntax error on line %d!\n", line_number);\
  destroy_vec_cmd(out);\
  out = NULL;\
  return NULL;\
}

// command struct (used for managing processes)
typedef struct {
  pid_t pid;
  int argc;
  // name is argv[0]
  char argv[max_args][max_line / max_args]; // TODO: THIS NEEDS OPTIMIZING
  int in_fd;
  int out_fd;
  int outerr_fd;
  bool bg;
  char filev[3][token_max_len];
} cmd_t;


// reset or initialize the struct
void init_cmd_t(cmd_t* val){
  val->pid = -1;
  val->argc = 0;
  val->in_fd = -1;
  val->out_fd = -1;
  val->outerr_fd = -1;
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
  vec_cmd* out = (vec_cmd*)malloc(sizeof(vec_cmd));
  if (out == NULL){
    perror("ERROR: allocating memory for process vector struct!");
    free(out);
    _exit(-1);
  }
  // initial capacity of 4 is reasonable (won't need to reallocate usually)
  out->capacity = 4;
  out->size = 0;
  out->values = (cmd_t*)malloc(sizeof(cmd_t) * out->capacity);

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
    // reallocate vector, we multiply capacity by two on each reallocation
    vector->capacity = vector->capacity * 2;
    vector->values = (cmd_t*)realloc(vector->values, vector->capacity * sizeof(cmd_t));
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

// some debugging stuff
void print_cmd(const cmd_t * command){
  printf("------------------------\n");
  printf("Program: %s\n", command->argv[0]);
  for (size_t j = 1; j < (size_t)command->argc; j++){
    printf("\tArgument: %s\n", command->argv[j]);
  }
  printf("File Descriptors:\n");
  printf("\tin: %d\n\tout: %d\n\touterr: %d\n\n", command->in_fd, command->out_fd, command->outerr_fd);
  printf("Filev:\n");
  printf("\t0: %s\n\t1: %s\n\t2: %s\n", command->filev[0], command->filev[1], command->filev[2]);
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

// Lexer states
typedef enum{
  LS_INITIAL,
  LS_BUILDING_WORD_QUOTES,
  LS_BUILDING_WORD,
  LS_BUILDING_ERR_REDIR
} LexerState;

// Token types
typedef enum{
  TOKEN_WORD,
  TOKEN_PIPE,
  TOKEN_REDIR_IN,
  TOKEN_REDIR_OUT,
  TOKEN_REDIR_ERR,
  TOKEN_NULL, // '\0'
  TOKEN_ERROR,
  TOKEN_BACKGROUND
} TokenType;

// token that gets passed to parser
typedef struct {
  // the type of token read
  TokenType token;
  // the actual text read (since we need to extract information on commands)
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

  // we can guarantee that this does not continue infinitely (probably hehehe)
  while(1){
    c = **input;
    switch(state){
      case LS_INITIAL:
        if(c == ' '){
          // consume character
          (*input) ++;
          continue;
        }
        else if(c == '|'){
          (*input) ++;
          return (Token){
            .token = TOKEN_PIPE,
            // TODO: check if it is neccessary here to set value for lexeme
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
        }else if(c == '&'){
          (*input) ++;
          return (Token){
            .token = TOKEN_BACKGROUND,
            .lexeme = ""
          };
        }
        else if(c == '!'){
          (*input) ++;
          state = LS_BUILDING_ERR_REDIR;
          continue;
        }else if (c == '\0'){
          return (Token){
            .token = TOKEN_NULL,
            .lexeme = "\0",
          };
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
        // a space, null terminator or reserverd character
        if(c == ' ' || c == '\0' || (is_nonreserved(c) == 0)){
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
        else if(is_nonreserved(c)){
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
        return (Token){
          .token = TOKEN_ERROR,
          .lexeme = "",
        };
        // break is redundant since we have already returned above but best practice
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
    }
  }
}

void exec_command(cmd_t command){
  pid_t pid;
  pid = fork();


  // case child
  if (pid == 0){
    char* exec_args[max_args];
    for (int i = 0; i < command.argc; i++){
      exec_args[i] = command.argv[i];
    }
    exec_args[command.argc] = NULL;
    // Replace STDIN
    if(command.in_fd != -1){
      int errstdin = close(STDIN_FILENO);
      if(errstdin < 0){
        perror("Error closing stdin!");
        _exit(-1);
      }

      int errdup = dup(command.in_fd);
      if(errdup < 0){
        perror("Error duping file descriptor!");
        _exit(-1);
      }

    }

    // Replace STDOUT
    if(command.out_fd != -1){
      int errstdin = close(STDOUT_FILENO);
      if(errstdin < 0){
        perror("Error closing stdout!");
        _exit(-1);
      }

      int errdup = dup(command.out_fd);
      if(errdup < 0){
        perror("Error duping file descriptor!");
        _exit(-1);
      }

    }

    // Replace STDERR
    if(command.outerr_fd != -1){
      int errstdin = close(STDERR_FILENO);
      if(errstdin < 0){
        perror("Error closing stderr!");
        _exit(-1);
      }

      int errdup = dup(command.outerr_fd);
      if(errdup < 0){
        perror("Error duping file descriptor!");
        _exit(-1);
      }
    }

    // execute the actual program
    execvp(exec_args[0], exec_args);
    perror("Couldn't execute command correctly!");
  }
  // case parent
  else{
    if (command.in_fd != -1) {
      close(command.in_fd);
    }
    if (command.out_fd != -1) {
      close(command.out_fd);
    }
    if (command.outerr_fd != -1) {
      close(command.outerr_fd);
    }
  }

}



// once we have parsed the line into cmd_t, we can proceed to execute them
void exec_line(vec_cmd* parsed_line){
  print_vec_cmd(parsed_line);
  return;
  for (size_t i = 0; i < parsed_line->size; i ++){
    exec_command(parsed_line->values[i]);
  }

  // wait for children
  for (size_t i = 0; i < parsed_line->size; i++){
    wait(NULL);
  }
}




typedef enum{
  PS_EXPECT_FILENAME_OUT,
  PS_EXPECT_FILENAME_ERR,
  PS_EXPECT_FILENAME_INP,
  PS_EXPECT_ARGS,
  PS_EXPECT_CMD,
  PS_EXPECT_CMD_PIPED
} ParserState;


// line to vector of processes
vec_cmd* parse_line(char* line, int line_number){
  line_number ++; // so that our debug messasges aren't 0 indexed for lines
  strip(line);

  size_t line_len = strlen(line);
  // skip empty or whitespace lines
  if (line_len == 0){
    return NULL;
  }

  // check that the first line is as specified
  if (line_number == 1){
    if (strcmp(line, "## Uc3mshell P2") != 0){
      // TODO: preguntar profe si esto deberia ser perror como en el enunciado
      fprintf(stderr,"ERROR: Unexpected first line!\n");
      _exit(-1);
    }else{
      return NULL;
    }
  }
  // if line is comment, skip
  if (line[0] == '#'){
    return NULL;
  }

  // the vector we return
  vec_cmd* out = create_vec_cmd();


  // pointer for lexer input
  char *store = line;
  Token latest_token;
  ParserState parser_state = PS_EXPECT_CMD;
  cmd_t current_command;
  init_cmd_t(&current_command);

  // 0 to read, 1 to write
  int pipe_fd[2];

  // we get a TOKEN_NULL once we have reached the end of the input
  while((latest_token = get_next_token(&store)).token != TOKEN_NULL){
    switch(latest_token.token){
      case TOKEN_ERROR:
        // ParserSyntaxError macro returns automatically
        ParserSyntaxError(line_number, out)
        break;
      case TOKEN_PIPE:
        if(parser_state == PS_EXPECT_CMD || parser_state == PS_EXPECT_CMD_PIPED){
          ParserSyntaxError(line_number, out)
        }
        pipe(pipe_fd);
        current_command.out_fd = pipe_fd[1];
        append_vec_cmd(out, current_command);
        init_cmd_t(&current_command);
        current_command.in_fd = pipe_fd[0];
        parser_state = PS_EXPECT_CMD_PIPED;
        break;
      case TOKEN_BACKGROUND:
        if (parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out)
        }
        current_command.bg = 1;
        break;
      case TOKEN_REDIR_OUT:
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out)
        }
        parser_state = PS_EXPECT_FILENAME_OUT;
        break;
      case TOKEN_REDIR_IN:
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out)
        }
        parser_state = PS_EXPECT_FILENAME_INP;
        break;
      case TOKEN_REDIR_ERR:
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out)
        }
        parser_state = PS_EXPECT_FILENAME_ERR;
        break;
      case TOKEN_WORD:
        if(parser_state == PS_EXPECT_CMD){
          strcpy(current_command.argv[0], latest_token.lexeme);
          current_command.argc = 1;
          parser_state = PS_EXPECT_ARGS;
        }else if (parser_state == PS_EXPECT_CMD_PIPED){
          strcpy(current_command.argv[0], latest_token.lexeme);
          current_command.argc = 1;
          parser_state = PS_EXPECT_ARGS;
        }else if(parser_state == PS_EXPECT_ARGS){
          strcpy(current_command.argv[current_command.argc++], latest_token.lexeme);
        }else if(parser_state == PS_EXPECT_FILENAME_INP){
          strcpy(current_command.filev[0], latest_token.lexeme);
          parser_state = PS_EXPECT_ARGS;
        }else if (parser_state == PS_EXPECT_FILENAME_OUT){
          strcpy(current_command.filev[1], latest_token.lexeme);
          parser_state = PS_EXPECT_ARGS;
        }else if(parser_state == PS_EXPECT_FILENAME_ERR){
          strcpy(current_command.filev[2], latest_token.lexeme);
          parser_state = PS_EXPECT_ARGS;
        }else{
          fprintf(stderr, "Okay this shouldn't be happening!");
          ParserSyntaxError(line_number, out)
        }
        break;
      case TOKEN_NULL:
        // We can't get this token but for completion of cases its included as a syntax error
        ParserSyntaxError(line_number, out);
        break;
    }
  }
  if(parser_state != PS_EXPECT_ARGS){
    ParserSyntaxError(line_number, out)
  }
  if (current_command.argc != 0){
    append_vec_cmd(out, current_command);
  }
  return out;
}

// open and process the file
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
  // close file
  int err = close(fd);
  if (err < 0){
    perror("Failed to close file!");
  }
}

void print_usage(char* bin_name){
  printf("Usage: %s <input_file>\n", bin_name);
}


int main(int argc, char *argv[]) {
  // we need a file name supplied
  if (argc != 2){
    print_usage(argv[0]);
    return -1;
  }
  process_file(argv[1]);
}
