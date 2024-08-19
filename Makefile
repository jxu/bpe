all: compress expand

compress: compress.c
	gcc -O2 -Wall -Wextra -std=c89 -pedantic -DDEBUG -o compress compress.c 

expand: expand.c
	gcc -O2 -Wall -Wextra -std=c89 -pedantic -DDEBUG -o expand expand.c


