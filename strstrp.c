#include <stddef.h>
#include <string.h>

int is_whitespace(char c){
  return c == ' ' || c == '\t' || c == '\n';
}


void strip_left(char* str){
  size_t i, j;
  size_t len = strlen(str);
  for (i = 0; i <= len; i ++){
    if(!(is_whitespace(str[i]))){
      break;
    }
  }
  for (j = 0; j < len - i; j++){
    str[j] = str[j + i];
  }
  str[j] = 0;
}

void strip_right(char* str){
  size_t i;
  for (i = strlen(str) - 1; ; i --){
    if (!is_whitespace(str[i])){
      break;
    }
  }
  str[++i] = 0;
}


void strip(char* str){
  str = (void*) str;
}


