CC=gcc
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
CLT=client

all: $(SRV) $(CLT)

build: all

$(SEL_SRV):$(SRV).c
	$(CC) -o $(SRV) $(LIBSOCKET) $(SRV).c

$(CLT):	$(CLT).c
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).c

clean:
	rm -f *.log
	rm -f $(SRV) $(CLT)


