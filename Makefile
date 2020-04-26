all: client.c server.c clientcommands.c servercommands.c networking.c sharedfunctions.c
	gcc -g -Wall client.c clientcommands.c networking.c sharedfunctions.c -lssl -lcrypto -o client
	gcc -g -Wall server.c servercommands.c networking.c sharedfunctions.c queue.c -pthread -lssl -lcrypto -o server

client: client.c clientcommands.c networking.c sharedfunctions.c
	gcc -g -Wall client.c clientcommands.c networking.c sharedfunctions.c -lssl -lcrypto -o client

server: server.c servercommands.c networking.c sharedfunctions.c
	gcc -g -Wall server.c servercommands.c networking.c sharedfunctions.c queue.c -pthread -lssl -lcrypto -o server

clean: 
	rm server
	rm client

setup:
	cp client ../testclient/
	cp server ../testserver/

