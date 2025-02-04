#
#  usbimager/Makefile
#
#  Copyright (C) 2020 bzt (bztsrc@gitlab)
#
#  Permission is hereby granted, free of charge, to any person
#  obtaining a copy of this software and associated documentation
#  files (the "Software"), to deal in the Software without
#  restriction, including without limitation the rights to use, copy,
#  modify, merge, publish, distribute, sublicense, and/or sell copies
#  of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
#  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#
#  @brief Project makefile
#

####### overall configuration #######

TARGET = usbimager
CC = gcc
LD = gcc
STRIP = strip
# setting DISKS_TEST to 1 will add a test.bin "device" to the target disks list
CFLAGS = -DDISKS_TEST=0 -D_FILE_OFFSET_BITS=64 -D__USE_FILE_OFFSET64 -D__USE_LARGEFILE -Wall -Wextra -pedantic --std=c99 -O3 -fvisibility=hidden -I./zlib -I./bzip2 -I./xz -I./zstd
LDFLAGS =
LIBS =
DECOMPRESSORS = zlib/libz.a bzip2/libbz2.a xz/libxz.a zstd/libzstd.a
PREFIX ?= usr/
INSTDIR=$(DESTDIR:/=)/$(PREFIX)

VERSION = $(shell cat main.h|grep USBIMAGER_VERSION|cut -d '"' -f 2)
SRC = $(filter-out $(wildcard main_*.c) $(wildcard disks_*.c),$(wildcard *.c))
OBJ =

####### detect operating system and platform #######

ifneq ("$(wildcard /bin/*.exe)","")
# Windows (mingw)
WIN = 1
SRC += main_win.c disks_win.c
CFLAGS += -DNDEBUG -DWINVER=0x0500 -DUNICODE=1
LDFLAGS += -mwindows -static -static-libgcc
TARGET = usbimager.exe
WINDRES = windres
ARCH = i686
ifeq ($(USE_WRONLY),)
OBJ += resource.o
else
OBJ += resourcewo.o
endif
ifneq ($(USE_PHY),)
CFLAGS+=-DUSE_PHY=1
FRM=win-gdi-phy
else
FRM=win-gdi
endif
else
ifneq ("$(wildcard /Library/Frameworks)","")
# MacOSX
MACOSX = 1
MYARCH = $(shell uname -m | sed s,amd,x86_,)
ifeq ($(MYARCH),x86_64)
MINVER=10.13
LIBUI=darwin.a
ARCH=intel
else
MINVER=11.0
LIBUI=darwin_arm.a
ARCH=arm
endif
ifneq ("$(wildcard /usr/bin/xcrun)","")
LDFLAGS+=-F$(shell xcrun --show-sdk-path)/System/Library/Frameworks/ -L$(shell xcrun --show-sdk-path)/usr/lib
else
LDFLAGS+=-F/Library/Developer/CommandLineTools/SDKs/MacOSX$(MINVER).sdk/System/Library/Frameworks/ -L/Library/Developer/CommandLineTools/SDKs/MacOSX$(MINVER).sdk/usr/lib
endif
CFLAGS_MINVER=-mmacos-version-min=$(MINVER)
CFLAGS+=-DMACOSX=1 -x objective-c -fobjc-exceptions $(CFLAGS_MINVER)
LDFLAGS+=-macos_version_min $(MINVER) -framework CoreFoundation -framework IOKit -framework DiskArbitration
ifneq ($(USE_X11),)
SRC += main_x11.c
CFLAGS += -I/usr/include/X11 -I/opt/X11/include
LIBS += -lX11
FRM = macosx-x11
else
SRC += main_libui.c
CFLAGS += -pthread
LDFLAGS += -framework Foundation -framework Cocoa
LIBS += libui/$(LIBUI) -lc -lobjc -lpthread
FRM = macosx-cocoa
endif
SRC += disks_darwin.c
LD = ld
GRP = operator
else
# Linux
LINUX = 1
ARCH = $(shell uname -m)
ifeq ($(USE_LIBUI)$(USE_GTK)$(USE_TUI),)
SRC += main_x11.c
CFLAGS += -I/usr/include/X11
ifneq ($(USE_UNIFONT),)
CFLAGS += -DUSE_UNIFONT
UF = _uf
endif
LIBS += -lX11
FRM = linux-x11
GRP = disk
else
ifneq ($(USE_TUI),)
SRC += main_tui.c
FRM = linux-tui
GRP = disk
else
ifneq ($(USE_GTK),)
SRC += main_gtk.c
CFLAGS += $(shell pkg-config --cflags gtk+-3.0)
else
SRC += main_libui.c
ifeq ($(ARCH),x86_64)
LIBS += libui/linux.a
else
ifeq ($(ARCH),aarch64)
LIBS += libui/raspios.a
else
LIBS += libui/raspbian.a
endif
endif
CFLAGS += -pthread
LDFLAGS += -pthread
endif
ifneq ("$(wildcard /usr/bin/pkg-config)","")
LIBS += $(shell pkg-config --libs gtk+-3.0)
else
LIBS += -lgtk-3 -lgdk-3 -lgobject-2.0 -lglib-2.0
endif
FRM = linux-gtk
GRP =
endif
ifneq ($(USE_UDISKS2)$(USE_GTK),)
CFLAGS += -DUSE_UDISKS2=1 $(shell pkg-config --cflags udisks2) -I/usr/include/gio-unix-2.0
LIBS += $(shell pkg-config --libs udisks2)
endif
endif
SRC += disks_linux.c
TMP = $(ARCH:x86_64=amd64)
TMP2 = $(TMP:aarch64=arm64)
DEBARCH = $(TMP2:armv7l=armhf)
endif
endif

ifneq ($(USE_WRONLY),)
CFLAGS += -DUSE_WRONLY=1
WO = _wo
else
WO =
endif

OBJ += $(SRC:.c=.o)
ifneq ($(DEBUG),)
CFLAGS += -g
GRP =
endif

####### rules to compile #######

all: $(TARGET)

zlib/Makefile:
	@cd zlib && chmod +x ./configure
	@cd zlib && CFLAGS="$(CFLAGS_MINVER)" ./configure

zlib/libz.a: zlib/Makefile
	@make CFLAGS="$(CFLAGS_MINVER)" -C zlib libz.a

bzip2/libbz2.a:
	@make CFLAGS="$(CFLAGS_MINVER)" -C bzip2 libbz2.a

xz/libxz.a:
	@make CFLAGS="$(CFLAGS_MINVER)" -C xz libxz.a

zstd/libzstd.a:
	@make CFLAGS="$(CFLAGS_MINVER)" -C zstd libzstd.a ZSTD_LEGACY_SUPPORT=0 ZSTD_LIB_DICTBUILDER=0 ZSTD_LIB_DEPRECATED=0 \
		ZSTD_LIB_MINIFY=1 ZSTD_STATIC_LINKING_ONLY=1 ZSTD_STRIP_ERROR_STRINGS=1 DEBUGLEVEL=0

resource.o: misc/resource.rc
	$(WINDRES) misc/resource.rc -o resource.o

resourcewo.o: misc/resourcewo.rc
	$(WINDRES) misc/resourcewo.rc -o resourcewo.o

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

$(TARGET): $(DECOMPRESSORS) $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(DECOMPRESSORS) $(LIBS)
ifeq ($(DEBUG),)
	$(STRIP) $(TARGET)
endif
ifneq ($(GRP),)
	chgrp $(GRP) $(TARGET) || true
	chmod g+s $(TARGET) || true
	@(ls -la $(TARGET)|grep $(GRP)|grep sr) || printf "\n\nWARNING - Your user is not member of the '$(GRP)' group, can't grant access. Run the following two commands manually:\n\n  sudo chgrp $(GRP) $(TARGET)\n  sudo chmod g+s $(TARGET)\n\n"
endif

####### install and package creation #######

install: $(TARGET)
	@mkdir $(INSTDIR)/bin $(INSTDIR)/share $(INSTDIR)/share/applications $(INSTDIR)/share/man $(INSTDIR)/share/man/man8 $(INSTDIR)/share/icons $(INSTDIR)/share/icons/hicolor $(INSTDIR)/share/icons/hicolor/16x16 $(INSTDIR)/share/icons/hicolor/16x16/apps $(INSTDIR)/share/icons/hicolor/32x32 $(INSTDIR)/share/icons/hicolor/32x32/apps $(INSTDIR)/share/icons/hicolor/64x64 $(INSTDIR)/share/icons/hicolor/64x64/apps $(INSTDIR)/share/icons/hicolor/128x128 $(INSTDIR)/share/icons/hicolor/128x128/apps 2>/dev/null || true
ifneq ($(GRP),)
	install -m 2755 -g $(GRP) $(TARGET) $(INSTDIR)/bin
else
	install -m 755 $(TARGET) $(INSTDIR)/bin
endif
	cp misc/usbimager.desktop $(INSTDIR)/share/applications
	cp misc/usbimager.8.gz $(INSTDIR)/share/man/man8
	cp misc/icon128.png $(INSTDIR)/share/icons/hicolor/128x128/apps/usbimager.png
	cp misc/icon64.png $(INSTDIR)/share/icons/hicolor/64x64/apps/usbimager.png
	cp misc/icon32.png $(INSTDIR)/share/icons/hicolor/32x32/apps/usbimager.png
	cp misc/icon16.png $(INSTDIR)/share/icons/hicolor/16x16/apps/usbimager.png

package: $(TARGET)
ifneq ($(WIN),)
	@mkdir USBImager
	@cp $(TARGET) USBImager
	@rm ../usbimager_$(VERSION)$(WO)-$(ARCH)-$(FRM).zip 2>/dev/null || true
	zip -r -9 ../usbimager_$(VERSION)$(WO)-$(ARCH)-$(FRM).zip USBImager
	@rm -rf USBImager
else
ifneq ($(MACOSX),)
	@mkdir USBImager.app USBImager.app/Contents USBImager.app/Contents/MacOS USBImager.app/Contents/Resources
	@cp $(TARGET) USBImager.app/Contents/MacOS
	@cp misc/Info.plist USBImager.app/Contents
	@cp misc/usbimager.icns USBImager.app/Contents/Resources
	@rm ../usbimager_$(VERSION)$(WO)-$(ARCH)-$(FRM).zip 2>/dev/null || true
	zip -r -9 ../usbimager_$(VERSION)$(WO)-$(ARCH)-$(FRM).zip USBImager.app
	@rm -rf USBImager.app
else
	@mkdir bin share share/applications share/man share/man/man8 share/icons share/icons/hicolor share/icons/hicolor/16x16 share/icons/hicolor/16x16/apps share/icons/hicolor/32x32 share/icons/hicolor/32x32/apps share/icons/hicolor/64x64 share/icons/hicolor/64x64/apps share/icons/hicolor/128x128 share/icons/hicolor/128x128/apps
	@cp $(TARGET) bin
	@cp misc/usbimager.desktop share/applications
	@cp misc/usbimager.8.gz share/man/man8
	@cp misc/icon128.png share/icons/hicolor/128x128/apps/usbimager.png
	@cp misc/icon64.png share/icons/hicolor/64x64/apps/usbimager.png
	@cp misc/icon32.png share/icons/hicolor/32x32/apps/usbimager.png
	@cp misc/icon16.png share/icons/hicolor/16x16/apps/usbimager.png
	@rm ../usbimager_$(VERSION)$(WO)$(UF)-$(ARCH)-$(FRM).zip 2>/dev/null || true
	zip -r -9 ../usbimager_$(VERSION)$(WO)$(UF)-$(ARCH)-$(FRM).zip bin share
	@rm -rf bin share
endif
endif

deb: $(TARGET)
	@mkdir DEBIAN usr usr/bin usr/share usr/share/applications usr/share/man usr/share/man/man8 usr/share/icons usr/share/icons/hicolor usr/share/icons/hicolor/16x16 usr/share/icons/hicolor/16x16/apps usr/share/icons/hicolor/32x32 usr/share/icons/hicolor/32x32/apps usr/share/icons/hicolor/64x64 usr/share/icons/hicolor/64x64/apps usr/share/icons/hicolor/128x128 usr/share/icons/hicolor/128x128/apps
	@cp $(TARGET) usr/bin
	@cp misc/usbimager.desktop usr/share/applications
	@cp misc/usbimager.8.gz usr/share/man/man8
	@cp misc/icon128.png usr/share/icons/hicolor/128x128/apps/usbimager.png
	@cp misc/icon64.png usr/share/icons/hicolor/64x64/apps/usbimager.png
	@cp misc/icon32.png usr/share/icons/hicolor/32x32/apps/usbimager.png
	@cp misc/icon16.png usr/share/icons/hicolor/16x16/apps/usbimager.png
	@cat misc/deb_control | sed s/ARCH/$(DEBARCH)/g | sed s/SIZE/`du -s usr|cut -f 1`/g >DEBIAN/control
	@md5sum `find usr -type f` >DEBIAN/md5sums
	@cp ../LICENSE DEBIAN/copyright
	@echo "2.0" >debian-binary
	@tar -czvf data.tar.gz usr
ifeq ($(USE_LIBUI)$(USE_GTK),)
	@cp misc/deb_postinst DEBIAN/postinst
	@chmod +x DEBIAN/postinst
	@tar -C DEBIAN -czvf control.tar.gz control copyright md5sums postinst
else
	@tar -C DEBIAN -czvf control.tar.gz control copyright md5sums
endif
	@ar r ../usbimager_$(VERSION)$(WO)-$(DEBARCH).deb debian-binary control.tar.gz data.tar.gz
	@rm -rf debian-binary control.tar.gz data.tar.gz DEBIAN usr

####### cleanup #######

clean:
	rm $(TARGET) *.o *.bin zlib/*.o zlib/*.exe zlib/ztest* bzip2/*.o xz/*.o zstd/common/*.o zstd/decompress/*.o 2>/dev/null || true

distclean: clean
	@make -C zlib clean || true
	@make -C bzip2 clean || true
	@make -C xz clean || true
	@make -C zstd clean || true
	@rm zlib/Makefile zlib/*.log zlib/zlib.pc $(DECOMPRESSORS) 2>/dev/null || true

####### help #######

help:
	@printf "Configuration:\n"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "DEBUG=1" "compile with debug symbols"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_WRONLY=1" "compile the write-only interface"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_UNIFONT=1" "compile with embedded unifont (X11 only)"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_UDISKS2=1" "compile with libudisks2 (GTK only)"
	@printf "Interfaces:\n"
	@printf "    \033[1;37m%-20s\033[0m%s\n" "(default)" "compile native interface (GDI, Cocoa, etc.)"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_TUI=1" "compile text interface"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_X11=1" "compile X11 interface"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_LIBUI=1" "compile libui interface"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "USE_GTK=1" "compile GTK interface"
	@printf "Targets:\n"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "all" "compile the source"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "install" "compile and install"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "package" "create package zip"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "deb" "build Debian deb"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "clean" "clean repo but leave libs as-is"
	@printf "    \033[1;32m%-20s\033[0m%s\n" "distclean" "clean everything, including libs"
	@printf "Example:\n"
	@printf "    USE_WRONLY=1 USE_GTK=1 USE_UDISKS2=1 make clean all deb\n\n"
