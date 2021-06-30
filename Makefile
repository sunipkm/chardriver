obj-m = char_buf.o
all: module test

test:
	gcc readbuf.c -o readbuf.out 
	echo "Run echo -n Hello world > /dev/charDev to write"
	echo "Keep readbuf.out running to read"

module:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *.out