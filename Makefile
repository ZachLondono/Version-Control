client: client.c clientcommands.c	
	gcc -g client.c clientcommands.c -o client

server: server.c networking.c
	gcc server.c networking.c -o server

clean: server client
	rm server
	rm client


