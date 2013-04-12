all: demo-file-io fuse-test

demo-file-io: demo-file-io.c
	gcc -o demo-file-io demo-file-io.c

clean: 
	rm -f demo-file-io fuse-test

fuse-test: fuse-test.cpp
	g++ -Wall `pkg-config fuse --cflags --libs` -c fuse-test.cpp 
	gcc fuse-test.o  -o fuse-test `pkg-config fuse --cflags --libs` -lstdc++

start: all
	rm -rf testdir
	mkdir testdir
	./fuse-test -f testdir > out.txt &
	mount
	tail -f out.txt &

stop:
	( sudo umount testdir ; echo "")
	( killall -q fuse-test ; echo "")
	mount
	( killall -q tail ; echo "")
