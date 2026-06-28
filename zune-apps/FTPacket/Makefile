CC       = x86_64-aros-gcc
SYSROOT  = $(HOME)/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/Development
INCLUDES = -I$(SYSROOT)/include
LIBS     = -lmui
TARGET   = FTPacket

$(TARGET): ftpacket.c
	$(CC) --sysroot=$(SYSROOT) $(INCLUDES) -o $(TARGET) ftpacket.c $(LIBS)

install:
	cp $(TARGET) $(HOME)/Aros/arosbuilds/core-linux-x86_64-d/bin/linux-x86_64/AROS/C/

clean:
	rm -f $(TARGET)