SRC = connect4x4.c

CC = gcc

LIBRARY_PATHS = -std=c99

NAME = connect4x4

LIBRARIES = -lm -pthread

all : $(OBJS)
	$(CC) $(LIBRARY_PATHS) $(SRC) $(LIBRARIES) -o $(NAME)
