TQFTPSERV := tqftpserv

CFLAGS := -Wall -g -O2
LDFLAGS := -lqrtr

SRCS := tqftpserv.c

OBJS := $(SRCS:.c=.o)

$(TQFTPSERV): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

install: $(TQFTPSERV)
	install -D -m 755 $< $(DESTDIR)$(prefix)/bin/$<

clean:
	rm -f $(TQFTPSERV) $(OBJS)
