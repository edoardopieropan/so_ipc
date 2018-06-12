# Sources
SRCS= threads.c
OBJS=$(SRCS:.c=.o)
EXECUTABLE = threads.x

# Config
CC=gcc
CFLAGS= -c -std=gnu99 -g #std per compilare i for(int i…) e -g per il gdb
LD=gcc
LDFLAGS = -lpthread

# Target

all: $(EXECUTABLE)  
	@echo Building $(EXECUTABLE)

threads.x: threads.o
	@echo Building $@
	$(CC) -o $@ $< $(LDFLAGS)

threads.o: threads.c 
	@echo Building $@
	$(CC) $(CFLAGS) -o $@ $<

doc : threads.o
	@doxygen

clean:
	rm -f $(EXECUTABLE)
	rm -f $(OBJS)
	rm -rf html


.PHONY: all clean
