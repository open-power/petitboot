CC=gcc
TWIN_CFLAGS=$(shell pkg-config --cflags libtwin)
TWIN_LDFLAGS=$(shell pkg-config --libs libtwin)

LDFLAGS = 
CFLAGS = -O0 -ggdb -Wall

PARSERS = native

all: petitboot udev-helper

petitboot: petitboot.o devices.o
	$(CC) $(LDFLAGS) -o $@ $^

petitboot: LDFLAGS+=$(TWIN_LDFLAGS)
petitboot: CFLAGS+=$(TWIN_CFLAGS)

udev-helper: devices/udev-helper.o devices/params.o \
		$(foreach p,$(PARSERS),devices/$(p)-parser.o)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f petitboot
	rm -f udev-helper
	rm -f *.o devices/*.o
