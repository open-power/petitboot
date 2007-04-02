CC=gcc
TWIN_CFLAGS=$(shell pkg-config --cflags libtwin)
TWIN_LDFLAGS=$(shell pkg-config --libs libtwin)

LDFLAGS = $(TWIN_LDFLAGS)
CFLAGS = -O0 -ggdb -Wall $(TWIN_CFLAGS)

OBJFILES = petitboot.o devices.o

petitboot: $(OBJFILES)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f petitboot
	rm -f *.o
