main:
	gcc -Wall lab4.c -lfuse -o lab4 -D_FILE_OFFSET_BITS=64
run:
	./lab4 mountdir
stop:
	killall './lab4'
clean:
	rm -f lab4
	rm -f log.txt
	rm -f directories