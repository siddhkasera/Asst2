all: server client

server: 
	gcc -c server.c -lssl -lcrypto

client: 
	gcc -c client.c -lssl -lcrypto

clean:
	rm -f server; rm client.o
