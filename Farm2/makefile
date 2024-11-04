CC = gcc
CFLAGS += -std=c99 -Wall -pedantic -g -pthread -Iheader

TARGETS = generafile farm

.PHONY: all test clean
.SUFFIXES: .c .h

%.o: %c
	$(CC) $(CFLAGS) -c -o $@ $<

all : $(TARGETS)

test: generafile farm 
	  ./test.sh

generafile: generafile.c 
		gcc -std=c99 -o $@ $^

farm: ./src/main.o ./src/Collector.o ./src/MasterThread.o ./src/TaskQueue.o ./src/Worker.o ./src/Utils.o ./src/ThreadpoolWorker.o ./src/Tree.o ./src/Socket.o ./src/DirectoryManager.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	\rm -f *.o
	\rm -f *.dat
	\rm -f *.txt
	\rm -f -r testdir
	\rm -f generafile
	\rm -f ./farm2.sck
	\rm -f farm
	\rm -f src/*.o 