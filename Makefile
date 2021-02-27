VERSION = 1.5

PREFIX = /usr
MANPREFIX = $(PREFIX)/share/man

PKGNAME = gpp
COMMAND = gpp

PY = python
SHEBANG = /usr$(BIN)/env $(PY)


all: gpp

gpp: gpp.py
	env VERSION='$(VERSION)' SHEBANG='$(SHEBANG)' $(PY) "$<" < "$<" > "$@"

install: gpp
	install -dm755 -- "$(DESTDIR)$(PREFIX)/bin"
	install -m755 -- gpp "$(DESTDIR)$(PREFIX)/bin/$(COMMAND)"
	install -dm755 -- "$(DESTDIR)$(MANPREFIX)/man"
	install -m644 -- gpp.1 "$(DESTDIR)$(MANPREFIX)/man/$(COMMAND).1"

uninstall:
	-rm -- "$(DESTDIR)$(PREFIX)/bin/$(COMMAND)"
	-rm -- "$(DESTDIR)$(MANPREFIX)/man/$(COMMAND).1"

clean:
	-rm -rf -- gpp bin obj

.PHONY: all install uninstall clean
