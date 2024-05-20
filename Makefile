.PHONY: all clean
all: myftpc myftpd 

myftpc: myftpc.c myftp.h 
	gcc -Wall -Wextra -o myftpc myftpc.c 

myftpd: myftpd.c myftp.h
	gcc -Wall -Wextra -o myftpd myftpd.c 

clean:
	rm -r myftpc myftpd 
