CC=$(CROSS_COMPILE)gcc

TARGET = writer
SRCS = writer.c

all: $(TARGET)

$(TARGET): $(SRCS)
	@$(CC) -o $@ $^

clean:
	@rm -f $(TARGET)

.PHONY: all clean

