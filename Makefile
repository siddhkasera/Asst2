all: server client

server: Server/WTFServer.c
	gcc -o Server/WTFServer Server/WTFServer.c -pthread

client: WTF.c
	gcc -o WTF  WTF.c -lssl -lcrypto

test: WTFTest.c server client
	gcc -o WTFTest WTFTest.c
clean:
	rm -f WTFServer
	rm -f WTF
	rm -f WTFTest
