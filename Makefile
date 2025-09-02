# Makefile for lmp

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lncurses -lSDL2 -lSDL2_mixer -lmpg123 -lm

# Project name and source files
TARGET = lmplayer
SRCS = main.c functions.c config.c cJSON.c handle_command.c
OBJS = $(SRCS:.c=.o)

# Default installation prefix
# This can be overridden during installation
PREFIX = /usr/local

# Phony targets do not represent actual files
.PHONY: all clean install uninstall

# Default target: build the executable
all: $(TARGET)

# Link object files to create the final executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Generic rule to compile .c source files into .o object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Remove compiled files
clean:
	rm -f $(OBJS) $(TARGET)

# Install the executable
install: all
	install -d -m 755 $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/

# Uninstall the executable
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)