CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -static

obj := padconftodts.o

obj += mux2420.o
obj += mux2430.o
obj += mux34xx.o
obj += mux44xx.o


all: clean $(obj)
	$(CC) $(CFLAGS) -o padconftodts $(obj)

clean:
	rm -f *.o padconftodts
