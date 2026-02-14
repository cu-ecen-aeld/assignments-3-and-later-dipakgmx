CC=$(CROSS_COMPILE)gcc

TARGET = aesdsocket
SRCS = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRCS)
	@$(CC) -o $@ $^

clean:
	@rm -f $(TARGET)

.PHONY: all clean

