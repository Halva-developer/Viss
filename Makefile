# =============================================================================
# Viss Language Toolchain Makefile
# Version: 0.3.2 "Peppermint Butler"
# Supported Platforms: Linux, macOS, FreeBSD, MinGW/Windows
# =============================================================================

CXX ?= g++
CXXFLAGS ?= -std=c++20 -O3 -pipe -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations
LDFLAGS ?= -pthread -ldl
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share/viss
INCDIR ?= $(PREFIX)/include/viss
MIMEDIR ?= $(PREFIX)/share/mime/packages
APPLICATIONSDIR ?= $(PREFIX)/share/applications
ICONSDIR ?= $(PREFIX)/share/icons/hicolor/256x256/apps

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(OS),Windows_NT)
    TARGET = viss.exe
    VSC_TARGET = vsc.exe
    SSI_TARGET = ssi.exe
    WINFLAGS = -lgdiplus -lgdi32 -luser32 -lcomdlg32 -lshell32 -lole32 -lwinmm -lws2_32 -lwininet
    VISS_RES = resources/viss.res.o
    VSC_RES = resources/vsc.res.o
else ifeq ($(UNAME_S),Darwin)
    TARGET = viss
    VSC_TARGET = vsc
    SSI_TARGET = ssi
    LDFLAGS = -pthread
    VISS_RES =
    VSC_RES =
else
    TARGET = viss
    VSC_TARGET = vsc
    SSI_TARGET = ssi
    LDFLAGS = -pthread -ldl
    VISS_RES =
    VSC_RES =
endif

SRC = src/vissc.cpp
VSC_SRC = src/vsc_cli.cpp
SSI_SRC = src/ssi_cli.cpp
HDRS = src/viss_vm.hpp src/vsc.hpp src/ssi.hpp
INCLUDES = -I. -Ilibs -Isrc

all: build

build: $(TARGET) $(VSC_TARGET) $(SSI_TARGET)

resources/viss.res.o: resources/viss.rc resources/viss.ico
	windres -I resources resources/viss.rc -O coff -o resources/viss.res.o

resources/vsc.res.o: resources/vsc.rc resources/vsc.ico
	windres -I resources resources/vsc.rc -O coff -o resources/vsc.res.o

$(TARGET): $(SRC) $(HDRS) $(VISS_RES)
ifeq ($(OS),Windows_NT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) $(VISS_RES) -o $(TARGET) $(WINFLAGS)
else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET) $(LDFLAGS)
endif
	@echo "Build complete: $(TARGET)"

$(VSC_TARGET): $(VSC_SRC) $(HDRS) $(VSC_RES)
ifeq ($(OS),Windows_NT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(VSC_SRC) $(VSC_RES) -o $(VSC_TARGET) $(WINFLAGS)
else
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(VSC_SRC) -o $(VSC_TARGET) $(LDFLAGS)
endif
	@echo "Build complete: $(VSC_TARGET)"

$(SSI_TARGET): $(VSC_TARGET)
ifeq ($(OS),Windows_NT)
	cp $(VSC_TARGET) $(SSI_TARGET)
else
	ln -sf $(VSC_TARGET) $(SSI_TARGET)
endif
	@echo "Build complete: $(SSI_TARGET) (compat alias)"

clean:
	rm -rf bin $(TARGET) $(VSC_TARGET) $(SSI_TARGET) .viss_cache test_*.exe *.tmp.*

install: build
	@echo "Installing Viss & VSC to $(PREFIX)..."
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)/libs
	install -d $(DESTDIR)$(INCDIR)
	install -d $(DESTDIR)$(INCDIR)/libs
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -m 755 $(VSC_TARGET) $(DESTDIR)$(BINDIR)/$(VSC_TARGET)
	ln -sf $(VSC_TARGET) $(DESTDIR)$(BINDIR)/$(SSI_TARGET) 2>/dev/null || install -m 755 $(SSI_TARGET) $(DESTDIR)$(BINDIR)/$(SSI_TARGET)
	cp -r libs/* $(DESTDIR)$(DATADIR)/libs/
	cp -r libs/* $(DESTDIR)$(INCDIR)/
	cp -r libs/* $(DESTDIR)$(INCDIR)/libs/
	chmod -R a+rX $(DESTDIR)$(DATADIR) $(DESTDIR)$(INCDIR)
	@if [ -d packaging/linux ]; then \
		install -d $(DESTDIR)$(MIMEDIR); \
		install -d $(DESTDIR)$(APPLICATIONSDIR); \
		install -d $(DESTDIR)$(ICONSDIR); \
		cp packaging/linux/viss.xml $(DESTDIR)$(MIMEDIR)/ 2>/dev/null || true; \
		cp packaging/linux/viss.desktop $(DESTDIR)$(APPLICATIONSDIR)/ 2>/dev/null || true; \
		cp extensions/viss-vscode/icon.png $(DESTDIR)$(ICONSDIR)/viss.png 2>/dev/null || true; \
		which update-mime-database >/dev/null 2>&1 && update-mime-database $(PREFIX)/share/mime || true; \
		which update-desktop-database >/dev/null 2>&1 && update-desktop-database $(PREFIX)/share/applications || true; \
	fi
	@echo "Viss installed successfully. Run 'viss -v' to verify."

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DATADIR)
	rm -rf $(DESTDIR)$(INCDIR)
	rm -f $(DESTDIR)$(MIMEDIR)/viss.xml
	rm -f $(DESTDIR)$(APPLICATIONSDIR)/viss.desktop
	rm -f $(DESTDIR)$(ICONSDIR)/viss.png
	@echo "Viss uninstalled from $(PREFIX)."

deb:
	@chmod +x packaging/linux/build_deb.sh
	@./packaging/linux/build_deb.sh

tar:
	@chmod +x packaging/linux/build_tar.sh
	@./packaging/linux/build_tar.sh

test: build
	./$(TARGET) test

.PHONY: all build clean install uninstall deb tar test
