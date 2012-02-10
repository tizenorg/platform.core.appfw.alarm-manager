CC ?= gcc

TARGETS =  alarmmgr_unit_ts

PKGS = dlog glib-2.0 bundle appsvc alarm-service

LDFLAGS = `pkg-config --libs $(PKGS)`

CFLAGS = -I. `pkg-config --cflags $(PKGS)`
CFLAGS += -Wall

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGETS)
