#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFSIZE 512
#define PERM 0644

int main(int argc, char **argv) {
  int source_file, destination_file, bytes_read;
  char buf[BUFSIZE];

  if (argc != 3) {
    printf("Usage: %s <source_file> <destination_file>\n", argv[0]);
    return -1;
  }

  if ((source_file = open(argv[1], O_RDONLY)) < 0) {
    return -1;
  }

  if ((destination_file = creat(argv[2], PERM)) < 0) {
    close(source_file);
    return -1;
  }

  while ((bytes_read = read(source_file, buf, BUFSIZE)) > 0) {
    if (write(destination_file, buf, bytes_read) < bytes_read) {
      close(source_file);
      close(destination_file);
      return -1;
    }
  }

  if (bytes_read == -1) {
    return -1;
  }

  close(source_file);
  close(destination_file);
  return 0;
}
