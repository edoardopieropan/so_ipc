# Sources
SRCS= figlio.c processes.c
OBJS=$(SRCS:.c=.o)
EXECUTABLE = figlio.x processes.x

# Config
CC=gcc
CFLAGS= -c -std=gnu99 -g #std per compilare i for(int i…) e -g per il gdb
LD=gcc

# Target

all: $(EXECUTABLE)  
	@echo Building $(EXECUTABLE)

figlio.x: figlio.o
	@echo Building $@
	$(CC) -o $@ $<

processes.x: processes.o
	@echo Building $@
	$(CC) -o $@ $<

processes.o: processes.c 
	@echo Building $@
	$(CC) $(CFLAGS) -o $@ $< 

figlio.o: figlio.c 
	@echo Building $@
	$(CC) $(CFLAGS) -o $@ $<

doc : processes.o figlio.o
	@doxygen

clean:
	rm -f $(EXECUTABLE)
	rm -f $(OBJS)
	rm -rf html


.PHONY: all clean
