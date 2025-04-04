###########################
# Flags de la compilación #
###########################

CC=gcc

#########################
# Flags para debugging #
#########################

CFLAGS=-Wall -Wextra -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function -Wno-sign-conversion -Werror -fsanitize=undefined -std=gnu2x -O0 -ggdb

###############
# Compilación #
###############

PROG=syncDir

ALL: $(PROG)

$(PROG): main.o
	$(CC) $(CFLAGS) $(LIBS) -o $(PROG) main.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f $(PROG) *.o *~

