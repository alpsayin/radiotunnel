#
# Alp Sayin
# 22.05.2012
#

#declare the source files
SOURCES= 	ax25.c \
			devtag-allinone.c \
			lock.c \
			manchester.c \
			printAsciiHex.c \
			serial.c \
			tun_alloc.c \
			util.c \
			datacollection.c \
			tunclient.c

#use c compiler
CC=gcc
#compile only, show all warnings
CFLAGS=-c -Wall
#link the used libraries staticly
LDFLAGS=-static
#define sources
OBJECTS=$(SOURCES:.c=.o)
#name of the target executable
EXECUTABLE=radiotunnel
#install directory
INSTALL_DIR=/usr/bin


all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@


# rule to make a .o file from .c
# $< gives input file name
# $@ gives output file name
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)

run: all
	./$(EXECUTABLE)

# copy the executable to usr bin directory
# and set suid, so that when it runs it runs as root
install: all
	cp $(EXECUTABLE) $(INSTALL_DIR)/$(EXECUTABLE)

# remove the executable from the usr bin directory
uninstall:
	rm -rf $(INSTALL_DIR)/$(EXECUTABLE)
