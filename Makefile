# =============================================================================
# Viss Language Toolchain Makefile
# Version: 0.2.2 "Prismo & Cosmic Owl"
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
    WINFLAGS = -lgdiplus -lgdi32 -luser32 -lcomdlg32 -lshell32 -lole32 -lwinmm -lws2_32
else ifeq ($(UNAME_S),Darwin)
    TARGET = viss
    LDFLAGS = -pthread
else
    TARGET = viss
    LDFLAGS = -pthread -ldl
endif

SRC = src/vissc.cpp
INCLUDES = -I. -Ilibs -Isrc

all: build

build: $(TARGET)

$(TARGET): $(SRC)
ifeq ($(OS),Windows_NT)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET) $(WINFLAGS)
else
	mkdir -p bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o bin/$(TARGET) $(LDFLAGS)
endif
	@echo "Build complete: $(TARGET) ^_^"

clean:
	rm -rf bin/$(TARGET) $(TARGET) .viss_cache test_*.exe *.tmp.*

install: build
	@echo "Installing Viss to $(PREFIX)..."
	install -d $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(DATADIR)/libs
	install -d $(DESTDIR)$(INCDIR)
	install -m 755 bin/$(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	cp -r libs/* $(DESTDIR)$(DATADIR)/libs/
	cp -r libs/* $(DESTDIR)$(INCDIR)/
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
	@echo "Viss installed successfully! Run 'viss -v' to verify. :3"

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
ifeq ($(OS),Windows_NT)
	./$(TARGET) test
else
	./bin/$(TARGET) test
endif

.PHONY: all build clean install uninstall deb tar test
