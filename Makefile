
CC = @gcc

IDIR = src
ODIR = obj
EDIR = examples

CFLAGS = -I $(IDIR)

LIBS=-lm -lrt -fopenmp


all: $(IDIR)/*.c 
	$(CC) -Wall -g -o $(ODIR)/basic $(EDIR)/basic.c	$(IDIR)/*.c $(CFLAGS) $(LIBS) 
	$(CC) -Wall -g -o $(ODIR)/transport_latency $(EDIR)/transport_latency.c	$(IDIR)/*.c $(CFLAGS) $(LIBS) 


clean:
	rm -f $(ODIR)/*.o