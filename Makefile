.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)


all: gpp
gpp.o: gpp.c arg.h

gpp: gpp.o
	$(CC) -o $@ gpp.o $(LDFLAGS)

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

install: gpp
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man1"
	cp -- gpp "$(DESTDIR)$(PREFIX)/bin/gpp"
	cp -- gpp.1 "$(DESTDIR)$(MANPREFIX)/man1/gpp.1"

uninstall:
	-rm -- "$(DESTDIR)$(PREFIX)/bin/gpp"
	-rm -- "$(DESTDIR)$(MANPREFIX)/man1/gpp.1"

clean:
	-rm -rf -- gpp *.o *.su

.SUFFIXES:
.SUFFIXES: .o .c

.PHONY: all install uninstall clean
