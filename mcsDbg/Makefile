CFLAGS=-I/usr/include/python2.6

all: _mcs.so

clean:
	-@rm _mcs.so

_mcs.so: mcs.c follow.c follow.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^
