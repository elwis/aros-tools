CC = x86_64-aros-gcc
INCLUDES = -I$(HOME)/Aros/AROS-20260626-linux-x86_64-system/Developer/include
LIBS = -lmui

TARGET = hello

$(TARGET): hello.c
	$(CC) $(INCLUDES) -o $(TARGET) hello.c $(LIBS)

clean:
	rm -f $(TARGET)