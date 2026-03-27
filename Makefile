# Executables
TARGETS = uc3mshell mycp

TARGETS_DBG = uc3mshell_debug mycp_debug

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -O2


# Debug flags
CFLAGS_DBG = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -g

# Header files
DEPS = mycalc.h

# Objects
OBJ_SHELL = uc3mshell.o mycalc.o
OBJ_MYCP = mycp.o

OBJ_SHELL_DBG = uc3mshell_debug.o mycalc_debug.o
OBJ_MYCP_DBG = mycp_debug.o

# Defined targets
all: $(TARGETS)


debug: $(TARGETS_DBG)


uc3mshell: $(OBJ_SHELL)
	$(CC) $(CFLAGS) -o $@ $^

mycp: $(OBJ_MYCP)
	$(CC) $(CFLAGS) -o $@ $^

# Debug rule
%_debug.o: %.c $(DEPS)
	$(CC) $(CFLAGS_DBG) -c -o $@ $<


# Generic rule
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ_SHELL) $(OBJ_MYCP)  uc3mshell mycp uc3mshell_debug mycp_debug $(OBJ_SHELL_DBG) $(OBJ_MYCP_DBG) 


uc3mshell_debug: $(OBJ_SHELL_DBG)
	$(CC) $(CFLAGS_DBG) -o $@ $^


mycp_debug: $(OBJ_MYCP_DBG)
	$(CC) $(CFLAGS_DBG) -o $@ $^


.PHONY: all clean debug
