
CC = gcc
CFLAGS = -Wall -O2 `pkg-config --cflags ibus-1.0 glib-2.0`
LIBS = `pkg-config --libs ibus-1.0 glib-2.0`

TARGET = dkst-ime
OBJS = hangul.o hanja_dict.o engine.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

hangul.o: hangul.c hangul.h
	$(CC) $(CFLAGS) -c hangul.c

hanja_dict.o: hanja_dict.c hanja_dict.h
	$(CC) $(CFLAGS) -c hanja_dict.c

engine.o: engine.c hangul.h hanja_dict.h
	$(CC) $(CFLAGS) -c engine.c

clean:
	rm -f $(TARGET) $(OBJS)

