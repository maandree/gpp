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
	install -dm755 -- "$(DESTDIR)$(PREFIX)/bin"
	install -m755 -- gpp "$(DESTDIR)$(PREFIX)/bin/gpp"
	install -dm755 -- "$(DESTDIR)$(MANPREFIX)/man"
	install -m644 -- gpp.1 "$(DESTDIR)$(MANPREFIX)/man/gpp.1"

uninstall:
	-rm -- "$(DESTDIR)$(PREFIX)/bin/gpp"
	-rm -- "$(DESTDIR)$(MANPREFIX)/man/gpp.1"

clean:
	-rm -rf -- gpp *.o *.su

.SUFFIXES:
.SUFFIXES: .o .c

.PHONY: all install uninstall clean
