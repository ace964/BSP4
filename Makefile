obj-m += tzm.o
all: test_tzm.o	
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
		gcc -o test_tzm test_tzm.o
clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
		rm test_tzm

test_tzm: test_tzm.c
	gcc -c test_tzm.c
