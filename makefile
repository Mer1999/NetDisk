.PHONY:all
all:netdisk_server.o
	@gcc -o server netdisk_server.o -I /usr/include/mysql /usr/lib64/mysql/libmysqlclient.so
.PHONY:clean
clean:
	@rm server *.o *.log