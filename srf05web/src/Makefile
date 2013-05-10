##
# Make the srf05web binary
##

CC=mips-openwrt-linux-gcc
CFLAGS=-Wall -Werror 
LDFLAGS=-lpthread -ldl

all: srf05web

srf05web: mongoose.h mongoose.c srf05web.c
	$(CC) $(CFLAGS) mongoose.c srf05web.c $(LDFLAGS) -o srf05web 

clean:
	rm srf05web
