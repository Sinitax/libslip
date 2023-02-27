PREFIX ?= /usr/local
BINDIR ?= /bin

CFLAGS = -I include

ifeq "$(LIBSLIP_DEBUG)" "1"
CFLAGS += -g
endif

all: build/libslip.so build/libslip.a

clean:
	rm -rf build

build:
	mkdir build

build/libslip.a: src/slip.c include/slip.h | build
	$(CC) -o build/tmp.o src/slip.c $(CFLAGS) -r
	objcopy --keep-global-symbols=libslip.abi build/tmp.o build/fixed.o
	ar rcs $@ build/fixed.o

build/libslip.so: src/slip.c include/slip.h | build
	$(CC) -o $@ src/slip.c -fPIC $(CFLAGS) -shared -Wl,-version-script libslip.lds

install:
	install -m755 build/libslip.so -t "$(DESTDIR)$(PREFIX)$(BINDIR)"
	install -m755 build/libslip.a -t "$(DESTDIR)$(PREFIX)$(BINDIR)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)$(BINDIR)/libslip.so"
	rm -f "$(DESTDIR)$(PREFIX)$(BINDIR)/libslip.a"

.PHONY: all clean install uninstall
