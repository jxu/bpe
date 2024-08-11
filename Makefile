all: compress expand

compress: compress.c
	gcc -O2 -Wall -Wextra -std=c89 -pedantic -o compress compress.c 

expand: expand.c
	gcc -O2 -Wall -Wextra -std=c89 -pedantic -o expand expand.c


