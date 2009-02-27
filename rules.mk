

VPATH = $(srcdir)

CPPFLAGS += -I$(top_srcdir) -I$(top_srcdir)/lib -I$(builddir)

# we need paths to be overridable at build-time
DEFS += '-DPREFIX="$(prefix)"' '-DPKG_SHARE_DIR="$(pkgdatadir)"' \
	'-DLOCAL_STATE_DIR="$(localstatedir)"'

uis = ui/test/pb-test
#parsers = native yaboot kboot
parsers = kboot
artwork = background.jpg cdrom.png hdd.png usbpen.png tux.png cursor.gz

ifeq ($(PBTWIN),y)
	uis += ui/twin/pb-twin
endif

log_objs = lib/log/log.o
talloc_objs = lib/talloc/talloc.o
list_objs = lib/list/list.o
waiter_objs = lib/waiter/waiter.o
server_objs = lib/pb-protocol/pb-protocol.o
parser_objs = discover/parser.o discover/parser-utils.o \
	      $(foreach p, $(parsers), discover/$(p)-parser.o)

parser_test_objs = parser-test.o $(parser_objs)

all: $(uis) discover/pb-discover

# twin gui
ui/twin/pb-twin: LDFLAGS+=$(twin_LDFLAGS) $(LIBTWIN)
ui/twin/pb-twin: CFLAGS+=$(twin_CFLAGS)

pb_twin_objs = ui/twin/pb-twin.o ui/common/devices.o

ui/twin/pb-twin: $(pb_twin_objs)
	$(LINK.o) -o $@ $^

# test ui
pb_test_objs = ui/test/pb-test.o ui/common/discover-client.o \
	$(log_objs) $(talloc_objs) $(server_objs) $(list_objs)

ui/test/pb-test: $(pb_test_objs)
	$(LINK.o) -o $@ $^

# discovery daemon
#pb_discover_objs = discover/params.o discover/parser.o discover/paths.o \
#	      discover/yaboot-cfg.o \
#	      $(foreach p,$(parsers),discover/$(p)-parser.o)

pb_discover_objs = discover/pb-discover.o discover/udev.o \
		   discover/discover-server.o discover/device-handler.o \
		   discover/paths.o $(talloc_objs) $(server_objs) \
		   $(parser_objs) $(list_objs) $(waiter_objs) $(log_objs)

discover/pb-discover: $(pb_discover_objs)
	$(LINK.o) -o $@ $^


parser-test: $(parser_test_objs)
	$(LINK.o) -o $@ $^

install: all utils/99-petitboot.rules
	$(INSTALL) -d $(DESTDIR)$(sbindir)/
	$(INSTALL) discover/pb-discover $(uis) $(DESTDIR)$(sbindir)/
	$(INSTALL) -d $(DESTDIR)$(pkgdatadir)/artwork/
	$(INSTALL) $(addprefix $(top_srcdir)/ui/twin/artwork/,$(artwork)) \
		$(DESTDIR)$(pkgdatadir)/artwork/
	$(INSTALL) -d $(DESTDIR)$(pkgdatadir)/utils
	$(INSTALL) $(top_srcdir)/utils/99-petitboot.rules \
		 $(DESTDIR)$(pkgdatadir)/utils

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
	rm -f $(uis)
	rm -f $(pb_twin_objs) $(pb_test_objs)
	rm -f $(pb_discover_objs)
	rm -f discover/pb-discover
	rm -f ui/test/pb-test

maintainer-clean: clean
	-rm -f $(top_srcdir)/aclocal.m4
	-rm -rf $(top_srcdir)/autom4te.cache
	-rm -f $(top_srcdir)/config.h.in
	-rm -f $(top_srcdir)/configure
	-rm -f config.h
	-rm -f config.log
	-rm -f config.status
	-rm -f Makefile
	-rm -f $(PACKAGE)-$(VERSION).tar.gz

