PREFIX?=/usr
VERSION=0.0.1
PACKAGE=petitboot
CC=gcc
INSTALL=install
TWIN_CFLAGS?=$(shell pkg-config --cflags libtwin)
TWIN_LDFLAGS?=$(shell pkg-config --libs libtwin)

LDFLAGS =
CFLAGS = --std=gnu99 -O0 -ggdb -Wall '-DPREFIX="$(PREFIX)"'

PARSERS = native yaboot kboot
ARTWORK = background.jpg cdrom.png hdd.png usbpen.png tux.png cursor.gz

all: petitboot petitboot-udev-helper

petitboot: petitboot.o devices.o
	$(CC) $(LDFLAGS) -o $@ $^

petitboot: LDFLAGS+=$(TWIN_LDFLAGS)
petitboot: CFLAGS+=$(TWIN_CFLAGS)

petitboot-udev-helper: devices/petitboot-udev-helper.o devices/params.o \
		devices/parser.o devices/paths.o devices/yaboot-cfg.o \
		$(foreach p,$(PARSERS),devices/$(p)-parser.o)
	$(CC) $(LDFLAGS) -o $@ $^

parser-test: devices/parser-test.o devices/params.o devices/parser.o \
		devices/paths.o devices/yaboot-cfg.o \
		$(foreach p,$(PARSERS),devices/$(p)-parser.o)
	$(CC) $(LDFLAGS) -o $@ $^

devices/%: CFLAGS+=-I.

install: all
	$(INSTALL) -D petitboot $(DESTDIR)$(PREFIX)/sbin/petitboot
	$(INSTALL) -D petitboot-udev-helper \
		$(DESTDIR)$(PREFIX)/sbin/petitboot-udev-helper
	$(INSTALL) -Dd $(DESTDIR)$(PREFIX)/share/petitboot/artwork/
	$(INSTALL) -t $(DESTDIR)$(PREFIX)/share/petitboot/artwork/ \
		$(foreach a,$(ARTWORK),artwork/$(a))

dist:	$(PACKAGE)-$(VERSION).tar.gz

check:	parser-test
	devices/parser-test.sh

distcheck: dist
	tar -xvf $(PACKAGE)-$(VERSION).tar.gz
	cd $(PACKAGE)-$(VERSION) && make check

$(PACKAGE)-$(VERSION).tar.gz: $(PACKAGE)-$(VERSION)
	tar czvf $@ $^

$(PACKAGE)-$(VERSION): clean
	for f in $$(git-ls-files); do \
		d=$@/$$(dirname $$f); \
		mkdir -p $$d; \
		cp -a $$f $$d; \
	done
clean:
	rm -rf $(PACKAGE)-$(VERSION)
	rm -f petitboot
	rm -f petitboot-udev-helper
	rm -f *.o devices/*.o
