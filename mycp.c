#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFSIZE 512 // The size of the buffer to be read
#define PERM 0644   // The permissions for the created file

int main(int argc, char **argv){ // We copy the contents of argv[1] into argv[2]
  int source_file, destination_file, bytes_read;
  char buf[BUFSIZE];

  if (argc != 3) {
    printf("Usage: %s <source_file> <destination_file>\n", argv[0]);
    return -1; // There was an error in the number of parameters passed
  }

  if ((source_file = open(argv[1], O_RDONLY)) < 0) {
    return -1; // There was an error opening the file to be copied
  }

  if ((destination_file = creat(argv[2], PERM)) < 0) {
    close(source_file);
    return -1; // There was an error creating the copy
  }

  // Read the contents of the source file BUFSIZE bytes at a time
  while ((bytes_read = read(source_file, buf, BUFSIZE)) > 0) {
    // Write the contents of the buffer in the destination file
    if (write(destination_file, buf, bytes_read) < bytes_read) {
      close(source_file);
      close(destination_file);
      return -1; // There was an error copying the contents
    }
  }

  if (bytes_read == -1) {
    return -1; // There was an error reading the contents
  }

  close(source_file);
  close(destination_file);
  return 0;
}
