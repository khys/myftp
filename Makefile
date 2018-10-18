myftpc: myftpc.o
	gcc -o myftpc myftpc.o

myftps: myftps.o
	gcc -o myftps myftps.o

myftpc.o: myftpc.c myftp.h
	gcc -c myftpc.c

myftps.o: myftps.c myftp.h
	gcc -c myftps.c

clean:
	\rm myftpc myftps *.o
