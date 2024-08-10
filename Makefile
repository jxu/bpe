all: compress expand

compress: compress.c
	gcc -O2 -Wall -Wextra -o compress compress.c 

expand: expand.c
	gcc -O2 -Wall -Wextra -o expand expand.c


