/*  expand.c: BPE expand routine

    Pseudocode:
    While not end of file
        Read pair table from input
        While more data in block
            If stack empty, read byte from input
            Else pop byte from stack
            If byte in table, push pair on stack
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
#include <assert.h>
#include <limits.h>

#ifdef DEBUG
/* C99: DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__) */
    #define DEBUG_PRINT(x) fprintf x 
#else
    #define DEBUG_PRINT(x) do {} while (0)
#endif

unsigned char left[UCHAR_MAX+1], right[UCHAR_MAX+1];
unsigned char stack[UCHAR_MAX+1]; /* overflow? */


/* expand block */
/* return true if there are more blocks (doesn't end in EOF) */
int expand(FILE* infile, FILE* outfile)
{
    unsigned i, sp = 0;
    signed char count;
    unsigned char usize, lsize;
    unsigned short size;
    unsigned b = 0;

    /* reset pair table to defaults */
    for (i = 0; i <= UCHAR_MAX; ++i) {
        left[i] = i;
        right[i] = 0;
    }


    /* read compressed pair table */
    while(b <= UCHAR_MAX) { /* b = current table index */
        int c = getc(infile);
        if (c == EOF) return 0; /* last block */
        count = (signed char)c;

        DEBUG_PRINT((stderr, "b: %d Count: %d\n", b, count));

        assert(count != 0);

        /* negative count: skip forward by |count| then read a pair */
        if (count < 0) {
            b += -count;

            /* if not end table, read single pair */
            if (b <= UCHAR_MAX) {
                /* doesn't handle if file unexpectedly ends */
                left[b] = getc(infile);
                right[b] = getc(infile);
                DEBUG_PRINT((stderr, "Read single pair %02x%02x\n", 
                             left[b], right[b]));
                ++b;
            }
        }

        else { /* positive count: read |count| pairs */
            unsigned b_end = b + count;
            for (; b < b_end; ++b) {
                left[b]  = getc(infile);
                right[b] = getc(infile);
                DEBUG_PRINT((stderr, "Read pair %02x%02x\n", 
                             left[b], right[b]));
            }
        }
    }

    assert(b == UCHAR_MAX+1); /* counts valid */


    DEBUG_PRINT((stderr, "Pair table:\n"));
    for (b = 0; b <= UCHAR_MAX; ++b) {
        DEBUG_PRINT((stderr, "%02x:%02x%02x\t", b, left[b], right[b]));
    }
    DEBUG_PRINT((stderr, "\n"));
    
    /* read compressed buffer size */
    usize = getc(infile);
    lsize = getc(infile);
    size = (usize << 8) + lsize;

    DEBUG_PRINT((stderr, "size: %d(%02x%02x)\n", size, usize, lsize));

    /* write output, pushing pairs to stack */
    i = 0;
    while (i < size || sp) { /* more to read or stack non-empty */
        int c;
        if (sp == 0) { /* stack empty */
            c = getc(infile); /* read byte */
            DEBUG_PRINT((stderr, "read byte: %02x\n", c));
            ++i;
        } else {
            c = stack[--sp]; /* pop byte */
            DEBUG_PRINT((stderr, "pop byte: %02x\n", c));
        }

        if (c != left[c]) { /* pair in table */
            /* push pair */
            stack[sp++] = right[c];
            stack[sp++] = left[c];
            DEBUG_PRINT((stderr, "push pair %02x%02x\n", left[c], right[c]));
        } else { /* pair not in table */
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
