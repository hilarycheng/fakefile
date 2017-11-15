all:
	gcc -shared -fPIC src/fakefile.c -o fakesys.so -ldl

shell:
	LD_PRELOAD=./fakesys.so bash
