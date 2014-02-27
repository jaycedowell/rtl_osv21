CFLAGS = $(shell python-config --cflags)

LDFLAGS = $(shell python-config --ldflags)

decoder.so: decoder.o
	$(CC) -o decoder.so decoder.o -lm -shared $(LDFLAGS)

decoder.o: decoder.c
	$(CC) -c $(CFLAGS) -fPIC -o decoder.o decoder.c -O3

clean:
	rm -rf decoder.o decoder.so
