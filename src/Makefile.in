CFLAGS=-g -Wall -Wno-error #-DNDEBUG
LDFLAGS=-ldevmapper

all: denyfs

denyfs:

install:
	cp denyfs		/usr/sbin
	cp ../tools/dfstouch	/usr/sbin
	cp ../tools/dfsopen	/usr/sbin
	cp ../tools/dfsclose	/usr/sbin
	cp ../tools/dfstest	/usr/sbin

uninstall:
	rm /usr/sbin/denyfs
	rm /usr/sbin/dfstouch
	rm /usr/sbin/dfsopen
	rm /usr/sbin/dfsclose
	rm /usr/sbin/dfstest

clean:
	rm -f denyfs

.PHONY: clean
