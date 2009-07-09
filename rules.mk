
VPATH = $(srcdir)

CPPFLAGS += -I$(top_srcdir) -I$(top_srcdir)/lib -I$(builddir)
LDFLAGS += $(CFLAGS)

# we need paths to be overridable at build-time
DEFS += '-DPREFIX="$(prefix)"' '-DPKG_SHARE_DIR="$(pkgdatadir)"' \
	'-DLOCAL_STATE_DIR="$(localstatedir)"'

# programs
pb_discover = discover/pb-discover
pb_cui = ui/ncurses/pb-cui
pb_test = ui/test/pb-test
pb_twin = ui/twin/pb-twin
pb_event = utils/pb-event
parser_test = test/parser-test

# install targets and components
daemons = $(pb_discover)
parsers = event kboot yaboot
uis = $(pb_cui) $(pb_test)
tests = $(parser_test)
utils = $(pb_event)

ifeq ($(PBTWIN),y)
	uis += $(pb_twin)
endif

# other to install
artwork = background.jpg cdrom.png hdd.png usbpen.png tux.png cursor.gz
man8 = pb-cui.8 pb-discover.8 pb-event.8 petitboot.8
rules = utils/99-petitboot.rules
udhcpc = utils/udhcpc

# client/daemon lib objs
list_objs = lib/list/list.o
log_objs = lib/log/log.o
protocol_objs = lib/pb-protocol/pb-protocol.o
system_objs = lib/system/system.o
talloc_objs = lib/talloc/talloc.o
waiter_objs = lib/waiter/waiter.o

# daemon objs
parser_objs = discover/parser.o discover/parser-conf.o discover/paths.o \
	$(foreach p, $(parsers), discover/$(p)-parser.o)
discover_objs = discover/event.o discover/user-event.o discover/udev.o \
	discover/discover-server.o discover/device-handler.o discover/paths.o \
	discover/parser-utils.o

# client objs
ui_common_objs = ui/common/discover-client.o ui/common/joystick.o \
	ui/common/loader.o ui/common/ui-system.o ui/common/timer.o \
	ui/common/url.o
ncurses_objs = ui/ncurses/nc-scr.o ui/ncurses/nc-menu.o ui/ncurses/nc-ked.o \
	ui/ncurses/nc-cui.o
twin_objs = ui/twin/pb-twin.o

# Makefiles
makefiles = Makefile $(top_srcdir)/rules.mk

# object collections
lib_objs = $(list_objs) $(log_objs) $(protocol_objs) $(system_objs) \
	$(talloc_objs) $(waiter_objs)

daemon_objs = $(lib_objs) $(parser_objs) $(discover_objs)

client_objs = $(lib_objs) $(ui_common_objs)

all: $(uis) $(daemons) $(utils)

# ncurses cui
pb_cui_objs-y$(ENABLE_PS3) += ui/ncurses/pb-cui.o
pb_cui_objs-$(ENABLE_PS3) += ui/ncurses/ps3-cui.o ui/common/ps3.o
pb_cui_ldflags-$(ENABLE_PS3) += -lps3-utils

pb_cui_objs = $(client_objs) $(ncurses_objs) $(pb_cui_objs-y)
$(pb_cui_objs): $(makefiles)
$(pb_cui): LDFLAGS += $(pb_cui_ldflags-y) -lmenu -lform -lncurses

$(pb_cui): $(pb_cui_objs)
	$(LINK.o) -o $@ $^

# test ui
pb_test_objs = $(client_objs) ui/test/pb-test.o
$(pb_test_objs): $(makefiles)

$(pb_test): $(pb_test_objs)
	$(LINK.o) -o $@ $^

# twin gui
pb_twin_objs = $(client_objs) $(twin_objs) ui/twin/ps3-twin.o
$(pb_twin_objs): $(makefiles)

$(pb_twin): LDFLAGS+=$(twin_LDFLAGS) $(LIBTWIN)
$(pb_twin): CFLAGS+=$(twin_CFLAGS)

$(pb_twin): $(pb_twin_objs)
	$(LINK.o) -o $@ $^

# discovery daemon
pb_discover_objs = $(daemon_objs) discover/pb-discover.o
$(pb_discover_objs): $(makefiles)

$(pb_discover): $(pb_discover_objs)
	$(LINK.o) -o $@ $^

# utils
pb_event_objs = utils/pb-event.o
$(pb_event_objs): $(makefiles)

$(pb_event): $(pb_event_objs)
	$(LINK.o) -o $@ $^

# parser-test
parser_test_objs = $(lib_objs) $(parser_objs) test/parser-test.o
$(parser_test_objs): $(makefiles)

$(parser_test): $(parser_test_objs)
	$(LINK.o) -o $@ $^

parser-test: $(parser_test)

install: all $(rules) $(udhcpc)
	$(INSTALL) -d $(DESTDIR)$(sbindir)/
	$(INSTALL_PROGRAM) $(daemons) $(uis) $(utils) $(DESTDIR)$(sbindir)/
	$(INSTALL) -d $(DESTDIR)$(pkgdatadir)/artwork/
	$(INSTALL_DATA) $(addprefix $(top_srcdir)/ui/twin/artwork/,$(artwork)) \
		$(DESTDIR)$(pkgdatadir)/artwork/
	$(INSTALL) -d $(DESTDIR)$(pkgdatadir)/utils
	$(INSTALL_DATA) $(top_srcdir)/$(rules) $(DESTDIR)$(pkgdatadir)/utils
	$(INSTALL_DATA) $(top_srcdir)/$(udhcpc) $(DESTDIR)$(pkgdatadir)/utils
	$(INSTALL) -d $(DESTDIR)$(mandir)/man8/
	$(INSTALL_DATA) $(addprefix $(top_srcdir)/man/, $(man8)) \
		$(DESTDIR)$(mandir)/man8/

dist: $(PACKAGE)-$(VERSION).tar.gz

check: parser-test
	$(SHELL) test/parser-test.sh

distcheck: dist
	tar -xvf $(PACKAGE)-$(VERSION).tar.gz
	cd $(PACKAGE)-$(VERSION) && make check

$(PACKAGE)-$(VERSION).tar.gz: $(PACKAGE)-$(VERSION)
	tar czvf $@ $^

$(PACKAGE)-$(VERSION): clean
	for f in $$(git --git-dir=$(top_srcdir)/.git ls-files); do \
		d=$@/$$(dirname $$f); \
		mkdir -p $$d; \
		cp -a $(top_srcdir)/$$f $$d; \
	done

clean:
	rm -rf $(PACKAGE)-$(VERSION)
	rm -f $(uis)
	rm -f $(pb_cui_objs)
	rm -f $(pb_test_objs)
	rm -f $(pb_twin_objs)
	rm -f $(daemons)
	rm -f $(pb_discover_objs)
	rm -f $(utils)
	rm -f $(pb_event_objs)
	rm -f $(tests)
	rm -f $(parser_test_objs)

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
