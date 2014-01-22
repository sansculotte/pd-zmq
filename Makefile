CC=gcc
CFLAGS=-Wall -I../../src/ -O6 -funroll-loops -fomit-frame-pointer -rdynamic -shared -fPIC
LIBS=-lzmq -lc

# this local/lib thing is here, because i have two different versions
# of libzmq on my machine and need to explicitly select the right one

zmq.pd_linux:
	$(CC) $(CFLAGS) zmq.c $(LIBS) -o zmq.pd_linux -L/usr/local/lib

clean:
	rm zmq.pd_linux
