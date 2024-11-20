CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror 
LDFLAGS ?= -lpthread -lrt
TARGET ?= aesdsocket
VALGRIND_LOG := valgrind_output.txt

all : $(TARGET)

$(TARGET) : aesdsocket.o
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.o $(LDFLAGS)

aesdsocket.o : aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean : 
	rm -f $(TARGET) aesdsocket.o 
	rm -f valgrind-out.txt

.PHONY : all clean 


