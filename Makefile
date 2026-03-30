CC = gcc
CFLAGS = -Wall -Wextra -pedantic -I./src -g
SRC = main.c ht.c
OBJ = $(SRC:.c=.o)
TARGET = ht

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean