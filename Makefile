CC = clang
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wconversion -O2
TARGET = image-tool
SOURCE = image_tool.c

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

debug:
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -g -O0 $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)
