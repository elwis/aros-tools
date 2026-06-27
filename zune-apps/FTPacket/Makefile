CC       = x86_64-aros-gcc
AROSDIR  = $(HOME)/Aros/AROS-20260626-linux-x86_64-system
INCLUDES = -I$(AROSDIR)/Developer/include
LIBS     = -lmui

TARGET   = FTPacket
SRC      = ftpacket.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(INCLUDES) -o $(TARGET) $(SRC) $(LIBS)

install: $(TARGET)
	cp $(TARGET) $(AROSDIR)/Utilities/$(TARGET)

clean:
	rm -f $(TARGET)
