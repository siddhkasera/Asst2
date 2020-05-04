all: server client

server: WTFServer.c
	gcc -o WTFServer WTFServer.c -pthread

client: WTF.c
	gcc -o WTF  WTF.c -lssl -lcrypto

test: WTFTest.c
	gcc -o WTFTest WTFTest.c
clean:
	rm WTFServer
	rm WTF
	rm WTFTest