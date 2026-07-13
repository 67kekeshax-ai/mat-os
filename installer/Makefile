CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
             -Isrc/gui -Isrc/license -Isrc/disk -Isrc/install
LDFLAGS  := -lm

TARGET  := matos-installer
SRCDIR  := src
OBJDIR  := .obj

SRCS := $(SRCDIR)/main.cpp \
        $(SRCDIR)/gui/framebuffer.cpp \
        $(SRCDIR)/gui/widget.cpp \
        $(SRCDIR)/license/license.cpp \
        $(SRCDIR)/disk/disk.cpp \
        $(SRCDIR)/install/install.cpp

OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -rf $(OBJDIR) $(TARGET)
