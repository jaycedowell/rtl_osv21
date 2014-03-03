CFLAGS = $(shell python-config --cflags) $(shell pkg-config --cflags librtlsdr)

LDFLAGS = $(shell python-config --ldflags) $(shell pkg-config --libs librtlsdr)

decoder.so: decoder.o
	$(CC) -o decoder.so decoder.o -lm -shared $(LDFLAGS)

decoder.o: decoder.c
	$(CC) -c $(CFLAGS) -fPIC -o decoder.o decoder.c -O3

clean:
	rm -rf decoder.o decoder.so
