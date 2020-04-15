client: client.c clientcommands.c networking.c sharedfunctions.c
	gcc -g client.c clientcommands.c networking.c sharedfunctions.c -lssl -lcrypto -o client

server: server.c networking.c sharedfunctions.c
	gcc server.c networking.c sharedfunctions.c -lssl -lcrypto -o server

clean: server client
	rm server
	rm client


