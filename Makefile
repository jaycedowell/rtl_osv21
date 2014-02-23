CFLAGS = $(shell python-config --cflags)

LDFLAGS = $(shell python-config --ldflags)

_decode.so: decode.o
	$(CC) -o _decode.so decode.o -lm -shared $(LDFLAGS)

decode.o: decode.c
	$(CC) -c $(CFLAGS) -fPIC -o decode.o decode.c -O3

clean:
	rm -rf decode.o _decode.so
