#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

// The size of the buffer to be read
#define BUFSIZE 512

int main(int argc, char **argv){ 
  // We copy the contents of the file specified in argv[1] into the one in argv[2]
  int source_file, destination_file;
  ssize_t bytes_read;
  char buf[BUFSIZE];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s <source_file> <destination_file>\n", argv[0]);
    return -1; // There was an error in the number of parameters passed
  }

  if ((source_file = open(argv[1], O_RDONLY)) < 0) {
    perror("Couldn't open source file!");
    return -1; // There was an error opening the file to be copied
  }

  // default copy behaviour overwrites second operand
  if ((destination_file = open(argv[2], O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0) {
    perror("Couldn't open destination file!");
    close(source_file);
    return -1; // There was an error creating the copy
  }

  // Read the contents of the source file BUFSIZE bytes at a time
  while ((bytes_read = read(source_file, buf, BUFSIZE)) > 0) {
    // Write the contents of the buffer in the destination file
    if (write(destination_file, buf, (size_t)bytes_read) < 0) {
      perror("Couldn't write to destination file!");
      close(source_file);
      close(destination_file);
      return -1; // There was an error copying the contents
    }
  }

  if (bytes_read == -1) {
    return -1; // There was an error reading the contents
  }

  // close files
  close(source_file);
  close(destination_file);

  return 0;
}
