CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
LIBS = -lm

ALL = serwis pracownik_serwisu kierowca mechanik kierownik kasjer

all: $(ALL)

serwis: src/main.c src/serwis_ipc.c 
	$(CC) $(CFLAGS) -o serwis src/main.c src/serwis_ipc.c

pracownik_serwisu: src/pracownik_serwisu.c src/serwis_ipc.c
	$(CC) $(CFLAGS) -o pracownik_serwisu src/pracownik_serwisu.c src/serwis_ipc.c
	
kierowca: src/kierowca.c src/serwis_ipc.c 
	$(CC) $(CFLAGS) -o kierowca src/kierowca.c src/serwis_ipc.c

mechanik: src/mechanik.c src/serwis_ipc.c 
	$(CC) $(CFLAGS) -o mechanik src/mechanik.c src/serwis_ipc.c

kierownik: src/kierownik.c src/serwis_ipc.c 
	$(CC) $(CFLAGS) -o kierownik src/kierownik.c src/serwis_ipc.c

kasjer: src/kasjer.c src/serwis_ipc.c 
	$(CC) $(CFLAGS) -o kasjer src/kasjer.c src/serwis_ipc.c

clean:
	rm -f $(ALL)