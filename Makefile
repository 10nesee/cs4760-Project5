# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Targets
TARGETS = oss user_proc

# Source files
OSS_SRC = oss.c
USER_PROC_SRC = user_proc.c

# Compilation rules
all: $(TARGETS)

oss: $(OSS_SRC)
	$(CC) $(CFLAGS) -o oss $(OSS_SRC)

user_proc: $(USER_PROC_SRC)
	$(CC) $(CFLAGS) -o user_proc $(USER_PROC_SRC)

# Clean rule
clean:
	rm -f $(TARGETS) *.o

