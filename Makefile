OBJS = connect4x4.c

CC = gcc

LIBRARY_PATHS = -std=c99

OBJ_NAME = connect4x4

LIBRARIES = -lm -pthread

all : $(OBJS)
	$(CC) $(LIBRARY_PATHS) $(OBJS) $(LIBRARIES) -o $(OBJ_NAME)
