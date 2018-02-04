PROG=test
OBJS=src/packet_frame.o src/packet.o src/main.o
CFLAGS=-Wall -pedantic -O3 -flto -fuse-linker-plugin

$(PROG): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) -lreactor -ldynamic

clean:
	rm -f $(PROG) $(OBJS)

