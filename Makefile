PREFIX?=/usr
CC=gcc
INSTALL=install
TWIN_CFLAGS=$(shell pkg-config --cflags libtwin)
TWIN_LDFLAGS=$(shell pkg-config --libs libtwin)

LDFLAGS = 
CFLAGS = -O0 -ggdb -Wall '-DPREFIX="$(PREFIX)"'

PARSERS = native
ARTWORK = background.png cdrom.png hdd.png usbpen.png cursor

all: petitboot udev-helper

petitboot: petitboot.o devices.o
	$(CC) $(LDFLAGS) -o $@ $^

petitboot: LDFLAGS+=$(TWIN_LDFLAGS)
petitboot: CFLAGS+=$(TWIN_CFLAGS)

udev-helper: devices/udev-helper.o devices/params.o \
		$(foreach p,$(PARSERS),devices/$(p)-parser.o)
	$(CC) $(LDFLAGS) -o $@ $^

devices/%: CFLAGS+=-I.

install: all
	$(INSTALL) -D petitboot $(PREFIX)/sbin/petitboot
	$(INSTALL) -D udev-helper $(PREFIX)/sbin/udev-helper
	$(INSTALL) -Dd $(PREFIX)/share/petitboot/artwork/
	$(INSTALL) -t $(PREFIX)/share/petitboot/artwork/ \
		$(foreach a,$(ARTWORK),artwork/$(a))

clean:
	rm -f petitboot
	rm -f udev-helper
	rm -f *.o devices/*.o
