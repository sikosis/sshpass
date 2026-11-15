CC ?= cc
CFLAGS ?= -O2
CFLAGS += -Wall -Wextra -Wpedantic
PREFIX ?= /boot/system/non-packaged
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/documentation/man/man1

.PHONY: all clean install test

all: sshpass

sshpass: sshpass.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

test: sshpass
	sh tests/test.sh

install: sshpass
	install -d "$(DESTDIR)$(BINDIR)"
	install -m 755 sshpass "$(DESTDIR)$(BINDIR)/sshpass"
	install -d "$(DESTDIR)$(MANDIR)"
	install -m 644 sshpass.1 "$(DESTDIR)$(MANDIR)/sshpass.1"

clean:
	rm -f sshpass
