#include "mycalc.h" // Includes mycalc.h
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// TODO: eliminar todo de abajo y este
// TODO: eliminar TODOs
#define max_commands 10
#define max_args 15
#define max_line 1024
#define token_max_len 256
#define arg_max_len (max_line / max_args)
// File Read Buffer size. Must be at least max_line
#define F_RD_BUF_SIZE 1024
#define true 1
#define false 0
#define bool int


// Really doesn't need to be a macro but i wanted to mess around
// with macros
#define ParserSyntaxError(line_number, out, current_command) { \
  fprintf(stderr, "Syntax error on line %d!\n", (line_number));\
  destroy_vec_cmd((out));\
  out = NULL;\
  if(current_command.in_fd != -1){\
    if (close(current_command.in_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  if(current_command.out_fd != -1){\
    if (close(current_command.out_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  if(current_command.outerr_fd != -1){\
    if (close(current_command.outerr_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  return NULL;\
}

// parser syntax error with message
#define ParserSyntaxErrorMsg(line_number, message, out, current_command) { \
  fprintf(stderr, "Syntax error on line %d: "message"!\n", (line_number));\
  destroy_vec_cmd((out));\
  out = NULL;\
  if(current_command.in_fd != -1){\
    if (close(current_command.in_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  if(current_command.out_fd != -1){\
    if (close(current_command.out_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  if(current_command.outerr_fd != -1){\
    if (close(current_command.outerr_fd) < 0){ \
      perror("[ERROR] Encountered an error while closing pipe!"); \
      _exit(-1); \
    } \
  } \
  return NULL;\
}

// Once again, does not need to be a macro but is cleaner i think
// replaces fd1 with fd2, if, and only if, f2 is a valid file descriptor
#define ReplaceFD(fd1, fd2) { \
  if ((fd2) != -1){ \
    int errdup = dup2((fd2), (fd1)); \
    if(errdup < 0){ \
      fprintf(stderr, "For file descriptor %d", (fd2)); \
      perror("error duping file descriptor!"); \
      _exit(-1); \
    } \
  } \
}


// string strip functionality
int is_whitespace(char c){
  return c == ' ' || c == '\t' || c == '\n';
}


void strip_left(char* str){
  if (str == NULL || *str == '\0') {
    return; 
  }
  size_t i, j;
  size_t len = strlen(str);
  for (i = 0; i <= len; ++ i){
    if(!(is_whitespace(str[i]))){
      break;
    }
  }
  for (j = 0; j < len - i; j++){
    str[j] = str[j + i];
  }
  str[j] = 0;
}

void strip_right(char* str) {
  if (str == NULL || *str == '\0') {
    return; 
  }
  size_t len = strlen(str);
  while (len > 0 && is_whitespace(str[len - 1])) {
    len--;
  }
  str[len] = '\0';
}


void strip(char* str){
  strip_left(str);
  strip_right(str);
}



// utility function to close a file descriptor and print an error if it fails (does not exit)
void close_print_error(int fd){
  int err = close(fd);
  if(err < 0){
    perror("[ERROR] Encountered an error while closing file!");
    _exit(-1);
  }
}


// command struct (used for managing processes)
typedef struct {
  pid_t pid;
  int argc;
  // name is argv[0]
  char argv[max_args][arg_max_len]; // this is huge but it needs to be this way
  int in_fd;
  int out_fd;
  int outerr_fd;
  char filev[3][token_max_len];
} cmd_t;


// reset or initialize the struct
void init_cmd_t(cmd_t* val){
  val->pid = -1;
  val->argc = 0;
  val->in_fd = -1;
  val->out_fd = -1;
  val->outerr_fd = -1;
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
  // should we run this chain of commands in the background?
  bool bg;
} vec_cmd;


// create the vector of commands
vec_cmd* create_vec_cmd(){
  vec_cmd* out = (vec_cmd*)malloc(sizeof(vec_cmd));
  if (out == NULL){
    perror("[ERROR] Couldn't allocate memory for process vector struct!");
    // we really can't continue running if we cannot allocate memory
    _exit(-1);
  }
  // initial capacity of 4 is reasonable (won't need to reallocate usually)
  out->capacity = 4;
  out->size = 0;
  out->values = (cmd_t*)malloc(sizeof(cmd_t) * out->capacity);
  out->bg = false;

  // check that the memory was correctly allocated
  if (out->values == NULL){
    perror("[ERROR] Allocating memory for process vector elements!");
    // we don't need a free here because memory will be cleaned up on exit
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
      perror("[ERROR] Couldn't reallocate memory for process vectors!");
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
  for (size_t i = 0; i < to_destroy->size; ++ i){
    // we need to make sure that we close all the file descriptors when we destroy this array
    if(to_destroy->values[i].in_fd != -1){
      close_print_error(to_destroy->values[i].in_fd);
    }
    if(to_destroy->values[i].out_fd != -1){
      close_print_error(to_destroy->values[i].out_fd);
    }
    if(to_destroy->values[i].outerr_fd != -1){
      close_print_error(to_destroy->values[i].outerr_fd);
    }
  }
  free(to_destroy->values);
  free(to_destroy);
}

// dynamically sized vector of process ids (used for backgrounding processes)
typedef struct{
  // pointer to first value
  pid_t* values;
  // number of items in list
  size_t size;
  // amount of items allocated
  size_t capacity;
} vec_pid;


// create the actual pid vector (allocated memory)
vec_pid* create_vec_pid(){
  vec_pid* out = (vec_pid*) malloc(sizeof(vec_pid));

  if (out == NULL){
    perror("[ERROR] Couldn't allocate memory for PID vector structure!");
    _exit(-1);
  }
  // we can have an initial capacity of 2 since this array is likey going to be small
  out->capacity = 2;
  out->size = 0;
  out->values = (pid_t*) malloc(sizeof(pid_t) * out->capacity);

  if(out->values == NULL){
    perror("[ERROR] Couldn't allocate memory for PID vector values!");
    _exit(-1);
  }

  return out;
}


// append a pid to the vector
void append_vec_pid(vec_pid* vector, pid_t value){
  if (vector->size == vector->capacity){
    vector->capacity = vector->capacity * 2;
    vector->values = (pid_t*) realloc(vector->values, sizeof(pid_t) * vector->capacity);
    if(vector->values == NULL){
      perror("[ERROR] Couldn't reallocate memory for vector of PIDs!");
      _exit(-1);
    }
  }
  vector->values[vector->size++] = value;
}

// remember to set pointer to NULL after
void destroy_vec_pid(vec_pid* vector){
  if(vector == NULL){
    return;
  }
  free(vector->values);
  free(vector);
}


vec_pid* bg_pids = NULL;

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

  for (size_t i = 0; i < vector->size; ++ i){
    print_cmd(&vector->values[i]);
  }
}

bool is_num(char* str){
  while(*str != '\0'){
    if(*str > '9' || *str < '0'){
      return false;
    }
    ++ str;
  }
  return true;
}



void exit_builtin(cmd_t command, vec_cmd* current_line){
  if (command.argc == 1){
    fprintf(stderr, "[ERROR] Missing exit code\n");
    return;
  }
  if (command.argc > 2){
    fprintf(stderr, "[ERROR] Too many arguments\n");
    return;
  }
  if(!is_num(command.argv[1])){
    fprintf(stderr, "[ERROR] The exit code must be an integer\n");
    return;
  }
  int ret_val = atoi(command.argv[1]);

  // wait for the rest of the processes in the current line
  for (size_t i = 0; i < current_line->size; ++ i){
    waitpid(current_line->values[i].pid, NULL, 0);
  }

  // wait for all backgrounded processes
  for(size_t i = 0; i < bg_pids->size; ++ i){
    waitpid(bg_pids->values[i], NULL, 0);
  }

  // clean up current line 
  destroy_vec_cmd(current_line);
  current_line = NULL;
  // clean up bg_pids
  destroy_vec_pid(bg_pids);
  // print ret_val
  printf("Goodbye %d\n", ret_val);
  _exit(ret_val);
}

void mycalc_builtin(int argc, char* argv[max_args]){
  mycalc(argc, argv);
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
          };
        }
        else if(c == '>'){
          (*input) ++;
          return (Token){
            .token = TOKEN_REDIR_OUT,
          };
        }
        else if(c == '<'){
          (*input) ++;
          return (Token){
            .token = TOKEN_REDIR_IN,
          };
        }else if(c == '&'){
          (*input) ++;
          return (Token){
            .token = TOKEN_BACKGROUND,
          };
        }
        else if(c == '!'){
          (*input) ++;
          state = LS_BUILDING_ERR_REDIR;
          continue;
        }else if (c == '\0'){
          return (Token){
            .token = TOKEN_NULL,
          };
        }
        else if(is_nonspace(c) && is_nonreserved(c)){
          state = LS_BUILDING_WORD;
          if (current_token_len >= token_max_len){
            fprintf(stderr, "[ERROR] Encountered a token that was too long while processing line!\n");
            return (Token){
              .token = TOKEN_ERROR,
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
        };
        break;
      case LS_BUILDING_ERR_REDIR:
        if (c == '>'){
          (*input)++;
          return (Token){
            .token = TOKEN_REDIR_ERR,
          };
        }
        return (Token){
          .token = TOKEN_ERROR,
        };
        break;
      case LS_BUILDING_WORD:
        // a space, null terminator or reserverd character
        if(c == ' ' || c == '\0' || (is_nonreserved(c) == 0)){
          Token out = {
            .token = TOKEN_WORD,
          };
          // make sure that we don't overflow buffer to append null terminator
          if(current_token_len >= token_max_len){
            fprintf(stderr, "[ERROR] Encountered a token that was too long while processing line!\n");
            return (Token){
              .token = TOKEN_ERROR,
            };
          }
          // put our buffer into the token struct
          buffer[current_token_len ++] = '\0';
          strcpy(out.lexeme, buffer);
          return out;
        }
        else if(is_nonreserved(c)){
          if (current_token_len >= token_max_len){
            fprintf(stderr, "[ERROR] Encountered a token that was too long while processing line!\n");
            return (Token){
              .token = TOKEN_ERROR,
            };
          }
          buffer[current_token_len++] = c;
          (*input) ++;
          continue;
        }
        return (Token){
          .token = TOKEN_ERROR,
        };
        // break is redundant since we have already returned above but best practice
        break;
      case LS_BUILDING_WORD_QUOTES:
        if(c == '"'){
          Token out = {
            .token = TOKEN_WORD,
          };
          // make sure that we don't overflow buffer to append null terminator
          if(current_token_len >= token_max_len){
            fprintf(stderr, "[ERROR] Encountered a token that was too long while processing line!\n");
            return (Token){
              .token = TOKEN_ERROR,
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
          };
        }else {
          // make sure that we don't overflow buffer to append new character
          if(current_token_len >= token_max_len){
            fprintf(stderr, "[ERROR] Encountered a token that was too long while processing line!\n");
            return (Token){
              .token = TOKEN_ERROR,
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

void exec_command(cmd_t* command){
  pid_t pid;
  pid = fork();


  // case child
  if (pid == 0){
    char* exec_args[max_args];
    for (int i = 0; i < command->argc; ++ i){
      exec_args[i] = command->argv[i];
    }
    exec_args[command->argc] = NULL;


    // Replace STDIN
    ReplaceFD(STDIN_FILENO, command->in_fd);
    // Replace STDOUT
    ReplaceFD(STDOUT_FILENO, command->out_fd);
    // Replace STDERR
    ReplaceFD(STDERR_FILENO, command->outerr_fd);

    if(strcmp(exec_args[0], "mycalc") == 0){
      mycalc_builtin(command->argc, exec_args);
      _exit(0);
    }


    // execute the actual program
    execvp(exec_args[0], exec_args);
    // fprintf(stderr, "%s: ", command->argv[0]);
    perror("Couldn't execute command correctly!");
    // exit CHILD (not parent)
    _exit(-1);
  }
  // case parent
  else{
    // update the command's pid
    command->pid = pid;
    if (command->in_fd != -1) {
      close_print_error(command->in_fd);
      command->in_fd = -1;
    }
    if (command->out_fd != -1) {
      close_print_error(command->out_fd);
      command->out_fd = -1;
    }
    if (command->outerr_fd != -1) {
      close_print_error(command->outerr_fd);
      command->outerr_fd = -1;
    }
  }
}



// once we have parsed the line into cmd_t, we can proceed to execute them
void exec_line(vec_cmd* parsed_line){
  // print_vec_cmd(parsed_line);
  // return;

  for (size_t i = 0; i < parsed_line->size; ++ i){
    // handle exit builtin
    if(strcmp(parsed_line->values[i].argv[0], "exit") == 0){
      exit_builtin(parsed_line->values[i], parsed_line);
    }
    else{
      // handle mycp command (prepend ./)
      if(strcmp(parsed_line->values[i].argv[0], "mycp") == 0){
        char temp[arg_max_len] = "./";
        strcat(temp, parsed_line->values[i].argv[0]);
        strcpy(parsed_line->values[i].argv[0], temp);
      }
      exec_command(&(parsed_line->values[i]));
    }
  }
  if(parsed_line->bg == false){
    // wait for children
    for (size_t i = 0; i < parsed_line->size; ++ i){
      // wait for only non-backgrounded processes
      waitpid(parsed_line->values[i].pid, NULL, 0);
    }
  }
  else{
    for (size_t i = 0; i < parsed_line->size; ++ i){
      append_vec_pid(bg_pids, parsed_line->values[i].pid);
    }
    // print the last element's pid
    printf("%d\n", parsed_line->values[parsed_line->size - 1].pid);
  }

}




typedef enum{
  PS_EXPECT_FILENAME_OUT,
  PS_EXPECT_FILENAME_ERR,
  PS_EXPECT_FILENAME_INP,
  PS_EXPECT_ARGS,
  PS_EXPECT_CMD,
  PS_EXPECT_CMD_PIPED,
  // Expect end of line
  PS_EXPECT_EOL
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
      perror("[ERROR] Unexpected first line!\n");
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
    if (parser_state == PS_EXPECT_EOL){
      ParserSyntaxErrorMsg(line_number, "expected end of line", out, current_command);
    }
    switch(latest_token.token){
      case TOKEN_ERROR:
        // ParserSyntaxError macro returns automatically
        ParserSyntaxError(line_number, out, current_command);
        break;
      case TOKEN_PIPE:
        // check we are not in an invalid state firstly
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out, current_command);
        }

        // then check that we haven't already specified an output file for our command
        if(current_command.filev[1][0] != '\0' /*for stdout*/ || current_command.filev[2][0] != '\0' /*for stderr*/){
          // returns automatically and includes the specified message in debug
          ParserSyntaxErrorMsg(line_number, "output redirection is already specified, cannot pipe output", out, current_command);
        }
        int err = pipe(pipe_fd);
        // error occured with pipe!
        if(err < 0){
          perror("[ERROR] Couldn't open pipe!");
          _exit(-1);
        }
        current_command.out_fd = pipe_fd[1];
        append_vec_cmd(out, current_command);
        // reset current command
        init_cmd_t(&current_command);
        current_command.in_fd = pipe_fd[0];
        parser_state = PS_EXPECT_CMD_PIPED;
        break;
      case TOKEN_BACKGROUND:
        if (parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out, current_command);
        }
        out->bg = true;
        parser_state = PS_EXPECT_EOL;
        break;
      case TOKEN_REDIR_OUT:
        // make sure we are in the right state
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out, current_command);
        }
        // make sure we haven't already supplied an output redirection
        if (current_command.filev[1][0] != '\0'){
          ParserSyntaxErrorMsg(line_number, "output redirection already supplied", out, current_command);
        }
        parser_state = PS_EXPECT_FILENAME_OUT;
        break;
      case TOKEN_REDIR_IN:
        // check state
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out, current_command);
        }
        // make sure that we haven't already specified an input
        if(current_command.in_fd != -1 || current_command.filev[0][0] != '\0'){
          ParserSyntaxErrorMsg(line_number, "cannot have input redirection, input already comes from pipe", out, current_command);
        }
        parser_state = PS_EXPECT_FILENAME_INP;
        break;
      case TOKEN_REDIR_ERR:
        if(parser_state != PS_EXPECT_ARGS){
          ParserSyntaxError(line_number, out, current_command);
        }
        // make sure we haven't already supplied an error redirection
        if (current_command.filev[2][0] != '\0'){
          ParserSyntaxErrorMsg(line_number, "output redirection already supplied", out, current_command);
        }
        parser_state = PS_EXPECT_FILENAME_ERR;
        break;
      case TOKEN_WORD:
        if(parser_state == PS_EXPECT_CMD){
          // copy command
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
          fprintf(stderr, "Okay this shouldn't be happening!\n");
          ParserSyntaxError(line_number, out, current_command);
        }
        break;
      case TOKEN_NULL:
        // We can't get this token but for completion of cases its included as a syntax error
        ParserSyntaxErrorMsg(line_number, "fatal error", out, current_command);
        break;
    }
  }
  if(parser_state != PS_EXPECT_ARGS && parser_state != PS_EXPECT_EOL){
    ParserSyntaxError(line_number, out, current_command);
  }
  if (current_command.argc != 0){
    append_vec_cmd(out, current_command);
  }
  return out;
}



bool resolve_file_redirections(vec_cmd* line){
  for (size_t i = 0; i < line->size; ++ i){
    // handle the file redirections
    // input redirection:
    if(line->values[i].filev[0][0] != '\0'){
      // if filev[0] is not an empty string, then we try to open the file...
      int fd = open(line->values[i].filev[0], O_RDONLY);
      if (fd < 0){
        perror("[ERROR] Couldn't open input redirection file!");
        return false;
      }
      line->values[i].in_fd = fd;
    }
    // output redirection:
    if(line->values[i].filev[1][0] != '\0'){
      int fd = open(line->values[i].filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0){
        perror("[ERROR] Couldn't open output redirection file!");
        return false;
      }
      line->values[i].out_fd = fd;
    }

    // error redirection:
    if(line->values[i].filev[2][0] != '\0'){
      int fd = open(line->values[i].filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0){
        perror("[ERROR] Couldn't open error output redirection file!");
        return false;
      }
      line->values[i].outerr_fd = fd;
    }
  }
  return true;
}

// open and process the file
void process_file(char* filename){
  int fd = open(filename, O_RDONLY);

  if (fd < 0){
    perror("[ERROR] Couldn't open input file");
    _exit(0);
  }

  char f_buf[F_RD_BUF_SIZE];
  char line[max_line] = {0};
  int line_number = 0;
  ssize_t bytes_read;
  size_t line_idx = 0; // Tracks our current position in the line buffer

  // read in chunks
  while ((bytes_read = read(fd, f_buf, F_RD_BUF_SIZE)) > 0){
    for (ssize_t i = 0; i < bytes_read; ++i){

      if (f_buf[i] == '\n'){
        line[line_idx] = '\0'; // Terminate the accumulated line
                               //
        // vector is freed once executed
        vec_cmd* parsed_line = parse_line(line, line_number++);
        if (parsed_line != NULL){
          // if we have successfully resolved the file redirections
          if(resolve_file_redirections(parsed_line)){
            exec_line(parsed_line);
          }
          destroy_vec_cmd(parsed_line);
        }

        line_idx = 0; // reset index for the next line
      }
      else {
        // make sure we have space
        if (line_idx < max_line - 1) {
            line[line_idx++] = f_buf[i];
        }
      }
    }
  }

  // handle a potential final line that didn't end in a newline
  if (line_idx > 0) {
    line[line_idx] = '\0';
    vec_cmd* parsed_line = parse_line(line, line_number++);
    if (parsed_line != NULL){
      if(resolve_file_redirections(parsed_line)){
        exec_line(parsed_line);
      }
      destroy_vec_cmd(parsed_line);
    }
  }

  // close file
  close_print_error(fd);
}


void sigchld_handler(int sig){
  // remove unused variable error
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    // wait on children
  }
}

void print_usage(char* bin_name){
  fprintf(stderr, "Usage: %s <input_file>\n", bin_name);
}


int main(int argc, char *argv[]) {
  // we need a file name supplied
  if (argc != 2){
    print_usage(argv[0]);
    return -1;
  }

  // setup handler for SIGCHLD signal
  struct sigaction act;
  // make sure no garbage data in act
  memset(&act, 0, sizeof(act));

  act.sa_handler = sigchld_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if (sigaction(SIGCHLD, &act, NULL) == -1) {
    perror("[ERROR] Couldn't set up SIGCHLD handler");
    _exit(-1);
  }

  bg_pids = create_vec_pid();
  process_file(argv[1]);

  for (size_t i = 0; i < bg_pids->size; ++ i){
    waitpid(bg_pids->values[i], NULL, 0);
  }
  destroy_vec_pid(bg_pids);
}
