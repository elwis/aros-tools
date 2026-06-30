.PHONY: all clean install

CC       = x86_64-aros-gcc
SYSROOT  = $(HOME)/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development
INCLUDES = -I$(SYSROOT)/include
LIBS     = -lmui
TARGET   = arossql

$(TARGET): arossql.c
	$(CC) --sysroot=$(SYSROOT) $(INCLUDES) \
		-O2 -Wall -fno-strict-aliasing -fno-common \
		-o $(TARGET) arossql.c $(LIBS)

clean:
	rm -f $(TARGET)
