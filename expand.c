/* expand.c: BPE expand routine

   Added some input validation to prevent crashes 
   Fuzzer found circular expansion issue in pair table

   Pseudocode 
   While not end of file
      Read pair table from input, checking pairs
      Check pair table for ciruclar expansion
      While more data in block
         If stack empty, read byte from input
         Else pop byte from stack
         If byte in table, push pair on stack, marking/checking seen
         Else write byte to output
      End while

   End while

   Compile:
      gcc -O3 -std=c89 -pedantic -Wall -Wextra -DDEBUG -o expand expand.c
   Usage:
      ./expand < test/sample.bpe > test/sample.new 2> test/expand.log
      cmp test/sample.txt test/sample.new
*/


#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#ifdef DEBUG
/* C99: DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__) */
    #define DEBUG_PRINT(x) fprintf x 
#else
    #define DEBUG_PRINT(x) (void)0
#endif

unsigned char lpair[UCHAR_MAX+1];
unsigned char rpair[UCHAR_MAX+1];
unsigned char stack[UCHAR_MAX]; /* only needs to be num unused bytes deep */
unsigned char  seen[UCHAR_MAX+1]; /* for expansion cycle detection */

/* getc that will exit safely with error message on EOF */
unsigned char safe_getc(FILE* infile, char errmsg[])
{
    int c = getc(infile);
    if (c == EOF) {
        fprintf(stderr, "%s\n", errmsg);
        exit(EXIT_FAILURE);
    }
    return (unsigned char)c; 
}

/* sanity check pair */
void check_pair(unsigned char b)
{
    /* not replaced pair should look like ff: ff 00 or ff 01 */
    if (b == lpair[b]) {            
        if (rpair[b] > 1) {
            fprintf(stderr, "Invalid not replaced pair\n");
            exit(EXIT_FAILURE);
        }
    /* replaced pair should look like ff: ee dd */
    } else {
        if (b == lpair[b] || b == rpair[b]) {
            fprintf(stderr, "Invalid replaced pair\n");
            exit(EXIT_FAILURE);
        }
    }
}

/* linear time toposort based on DFS */
void recurse_byte(unsigned char c)
{
    DEBUG_PRINT((stderr, "Recurse %02x\n", c));

    if (seen[c] == 2)  /* finished (black) */
        return;
    
    if (seen[c] == 1) { /* Visiting an already discovered node is a cycle */
        fprintf(stderr, "Circular byte expansion detected!\n");
        exit(EXIT_FAILURE);
    }

    seen[c] = 1; /* mark discovered (gray) */

    if (c != lpair[c]) { /* replaced pair, meaning non-leaf */
        recurse_byte(lpair[c]);
        recurse_byte(rpair[c]);
    }
    
    seen[c] = 2; /* done considering all descendants, mark finished */
}

/* expand block */
/* return true if there are more blocks (doesn't end in EOF) */
int expand(FILE* infile, FILE* outfile)
{
    unsigned i, sp = 0;
    signed char count;
    unsigned char usize, lsize;
    unsigned short size;
    unsigned b = 0;

    DEBUG_PRINT((stderr, "***** BEGIN BLOCK *****\n"));

    /* reset pair table to defaults */
    for (i = 0; i <= UCHAR_MAX; ++i) {
        lpair[i] = i;
        rpair[i] = 0;
    }

    /* read compressed pair table */
    /* b = current table index */
    while(b <= UCHAR_MAX) { 
        /* read count, checking for EOF on last block */
        int c = getc(infile);
        if (c == EOF) return 0;
        
        count = (signed char)c; /* pair table format uses signed byte */

        DEBUG_PRINT((stderr, "b: %d Count: %d\n", b, count));

        if (count == 0) {
            fprintf(stderr, "Bad count=0\n");
            exit(EXIT_FAILURE);
        }

        /* negative count: skip forward by |count| then read a pair */
        if (count < 0) {
            b += -count;

            /* if not end table, read single pair */
            if (b <= UCHAR_MAX) {
                lpair[b] = safe_getc(infile, "Missing left byte");
                rpair[b] = safe_getc(infile, "Missing right byte");
                DEBUG_PRINT((stderr, "Read single pair %02x%02x\n", 
                             lpair[b], rpair[b]));
                check_pair(b);
                ++b;
            }
        }

        else { /* positive count: read |count| pairs */
            unsigned b_end = b + count;
            for (; b < b_end; ++b) {
                lpair[b] = safe_getc(infile, "Missing left byte");
                rpair[b] = safe_getc(infile, "Missing right byte");
                DEBUG_PRINT((stderr, "Read pair %02x%02x\n", 
                             lpair[b], rpair[b]));
                check_pair(b);
            }
        }
    }

    if (b != UCHAR_MAX+1) {
        fprintf(stderr, "Invalid count sum\n");
        exit(EXIT_FAILURE);
    }


    DEBUG_PRINT((stderr, "Pair table:\n"));

    for (b = 0; b <= UCHAR_MAX; ++b) {
        DEBUG_PRINT((stderr, "%02x:%02x%02x ", b, lpair[b], rpair[b]));
        if (b % 8 == 7) DEBUG_PRINT((stderr, "\n"));
    }
    DEBUG_PRINT((stderr, "\n"));

    /* check pair table for circular expansion */
    for (i = 0; i <= UCHAR_MAX; ++i)
        seen[i] = 0;

    for (i = 0; i <= UCHAR_MAX; ++i) {
        if (seen[i] != 2) recurse_byte(i);
    }

    
    /* read compressed buffer size */
    usize = safe_getc(infile, "missing size bytes");
    lsize = safe_getc(infile, "missing size bytes");
    size = (usize << 8) + lsize;

    DEBUG_PRINT((stderr, "size: %d(%02x%02x)\n", size, usize, lsize));


    /* write output, pushing pairs to stack */
    i = 0;
    while (i < size || sp) { /* more to read or stack non-empty */
        unsigned char c;
        if (sp == 0) { /* stack empty */
            c = safe_getc(infile, "Unexpected buffer end"); /* read byte */
            DEBUG_PRINT((stderr, "read byte: %02x\n", c));
            ++i;
        } else {
            c = stack[--sp]; /* pop byte */
            DEBUG_PRINT((stderr, "sp=%d pop byte %02x\n", sp, c));
        }

        if (c != lpair[c]) { /* pair in table */
            /* push pair */
            stack[sp++] = rpair[c];
            stack[sp++] = lpair[c];
            DEBUG_PRINT((stderr, "sp=%d push pair %02x%02x\n", 
                         sp, lpair[c], rpair[c]));
            
        } else { 
            /* pair not in table */
            putc(c, outfile); /* write literal byte */
            DEBUG_PRINT((stderr, "write byte %02x\n", c));
        }
    }

    return 1; /* try another block */
}

int main(void)
{
    int notdone;
    /* process blocks */
    do {
        notdone = expand(stdin, stdout);
    } while (notdone);

    return 0;
}
