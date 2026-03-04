TARGET = codigo
SRCS = codigo.cpp
OBJS = $(SRCS:.c=.o)

INCLUDE =
LIBS = -lm -lpthread

CC ?= gcc
CFLAGS += -Wall -g $(INCLUDE)
LDFLAGS += $(LIBS)

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "[link] $^"
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	@echo "[compile] $<"
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)


