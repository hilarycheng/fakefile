all:
	gcc -shared -fPIC src/fakefile.c -o fakesys.so -ldl -lpthread
	g++ -g src/daemon.cpp -o fakedaemon -ldl

shell:
	LD_PRELOAD=./fakesys.so bash
