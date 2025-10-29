CC = gcc

CFLAGS = -Wall -g

all: meu_navegador meu_servidor

meu_navegador: cliente.c
	$(CC) $(CFLAGS) -o meu_navegador cliente.c

meu_servidor: servidor.c
	$(CC) $(CFLAGS) -o meu_servidor servidor.c -lpthread

clean:
	rm -f meu_navegador meu_servidor *.o