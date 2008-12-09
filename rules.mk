
VPATH = $(srcdir)

# we need paths to be overridable at build-time
DEFS += '-DPREFIX="$(prefix)"' '-DPKG_SHARE_DIR="$(pkgdatadir)"'

parsers = native yaboot kboot
artwork = background.jpg cdrom.png hdd.png usbpen.png tux.png cursor.gz

petitboot_objs = petitboot.o devices.o

parser_objs = devices/params.o devices/parser.o devices/paths.o \
	      devices/yaboot-cfg.o \
	      $(foreach p,$(parsers),devices/$(p)-parser.o)

petitboot_udev_helper_objs = devices/petitboot-udev-helper.o $(parser_objs)
parser_test_objs = parser-test.o $(parser_objs)

all: petitboot petitboot-udev-helper

petitboot: LDFLAGS+=$(twin_LDFLAGS)
petitboot: CFLAGS+=$(twin_CFLAGS)

petitboot: $(petitboot_objs)
	$(LINK.o) -o $@ $^

petitboot-udev-helper: $(petitboot_udev_helper_objs)
	$(LINK.o) -o $@ $^

parser-test: $(parser_test_objs)
	$(LINK.o) -o $@ $^

petitboot-udev-helper: CFLAGS+=-I$(top_srcdir)

install: all
	$(INSTALL) -D petitboot $(DESTDIR)$(sbindir)/petitboot
	$(INSTALL) -D petitboot-udev-helper \
		$(DESTDIR)$(sbindir)/petitboot-udev-helper
	$(INSTALL) -Dd $(DESTDIR)$(pkgdatadir)/artwork/
	$(INSTALL) -t $(DESTDIR)$(pkgdatadir)/artwork/ \
		$(foreach a,$(artwork),$(top_srcdir)/artwork/$(a))

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
