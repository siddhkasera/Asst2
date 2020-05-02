SUBDIRS := $(Server */.)

all: Server client

SUBDIRS:
	gcc -o WTF WTFServer.c -pthread

client:
	gcc -o WTF  WTF.c -lssl -lcrypto

clean:
	rm -f server; rm WTF.o
