CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
             -Isrc/wm -Isrc/taskbar -Isrc/startmenu \
             -Isrc/filemanager -Isrc/login -Isrc/bsod
LDFLAGS  := -lX11 -lXext -lcrypt -lm

OBJDIR   := .obj

# ─── matos-shell ────────────────────────────────────────────────────────────
SHELL_TARGET := matos-shell
SHELL_SRCS   := src/main.cpp \
                src/wm/window_manager.cpp \
                src/taskbar/taskbar.cpp \
                src/startmenu/startmenu.cpp \
                src/filemanager/filemanager.cpp \
                src/bsod/bsod.cpp

SHELL_OBJS := $(patsubst src/%.cpp,$(OBJDIR)/shell/%.o,$(SHELL_SRCS))

# ─── matos-login ────────────────────────────────────────────────────────────
LOGIN_TARGET := matos-login
LOGIN_SRCS   := src/login/login_main.cpp \
                src/login/login.cpp \
                src/bsod/bsod.cpp

LOGIN_OBJS := $(patsubst src/%.cpp,$(OBJDIR)/login/%.o,$(LOGIN_SRCS))

.PHONY: all clean install

all: $(SHELL_TARGET) $(LOGIN_TARGET)

$(SHELL_TARGET): $(SHELL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(LOGIN_TARGET): $(LOGIN_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/shell/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/login/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

install: all
	install -Dm755 $(SHELL_TARGET) /usr/local/bin/$(SHELL_TARGET)
	install -Dm755 $(LOGIN_TARGET)  /usr/local/bin/$(LOGIN_TARGET)
	mkdir -p /opt/matos-shell/assets
	cp -r assets/* /opt/matos-shell/assets/ 2>/dev/null || true

clean:
	rm -rf $(OBJDIR) $(SHELL_TARGET) $(LOGIN_TARGET)
