CC := gcc
CFLAGS := -Wall -Wextra
HEADERS := $(wildcard *.h)
OBJECTS := $(subst .h,.o,$(HEADERS))
TARGET := webserver

ifdef DEBUG
CFLAGS += -DDEBUG
endif

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

PHONY: clean distclean

clean:
	rm -f *.o

distclean: clean
	rm -f $(TARGET)
