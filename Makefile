CC = gcc
CFLAGS = -Wall -Wextra -std=c11

ALL = serwis pracownik_serwisu kierowca mechanik kierownik kasjer

all: $(ALL)

serwis_ipc.o: src/serwis_ipc.c src/serwis_ipc.h
	$(CC) $(CFLAGS) -c src/serwis_ipc.c -o serwis_ipc.o

serwis: src/main.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o serwis src/main.c serwis_ipc.o

pracownik_serwisu: src/pracownik_serwisu.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o pracownik_serwisu src/pracownik_serwisu.c serwis_ipc.o

kierowca: src/kierowca.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o kierowca src/kierowca.c serwis_ipc.o

mechanik: src/mechanik.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o mechanik src/mechanik.c serwis_ipc.o

kierownik: src/kierownik.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o kierownik src/kierownik.c serwis_ipc.o

kasjer: src/kasjer.c serwis_ipc.o src/serwis_ipc.h
	$(CC) $(CFLAGS) -o kasjer src/kasjer.c serwis_ipc.o

clean:
	rm -f $(ALL) *.o raport.txt

run: all
	./serwis