/* expand.c: BPE expand routine

   To test compress and expand:
   ../compress gcc.elf gcc.bpe > compress.log &&
   ../expand   gcc.bpe gcc.new > expand.log &&
   cmp gcc.elf gcc.new

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
*/


#include <stdio.h>
#include <assert.h>

#define DEBUG 0 /* debug flag */

unsigned char left[256], right[256];
unsigned char stack[256]; /* overflow? */


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
    for (i = 0; i < 256; ++i) {
        left[i] = i;
        right[i] = 0;
    }


    /* read compressed pair table */
    while(b < 256) { /* b = current table index */
        int c = getc(infile);
        if (c == EOF) return 0; /* last block */
        count = (signed char)c;

        if (DEBUG)
            printf("b: %d Count: %d\n", b, count);

        assert(count != 0);

        /* negative count: skip forward by |count| then read a pair */
        if (count < 0) {
            b += -count;

            /* if not end table, read single pair */
            if (b < 256) {
                /* doesn't handle if file unexpectedly ends */
                left[b] = getc(infile);
                right[b] = getc(infile);
                if (DEBUG)
                    printf("Read single pair %02x%02x\n", left[b], right[b]);
                ++b;
            }
        }

        else { /* positive count: read |count| pairs */
            unsigned b_end = b + count;
            for (; b < b_end; ++b) {
                left[b]  = getc(infile);
                right[b] = getc(infile);
                if (DEBUG)
                    printf("Read pair %02x%02x\n", left[b], right[b]);
            }
        }
    }

    assert(b == 256); /* counts valid */

    if (DEBUG) {
        printf("Pair table:\n");
        for (b = 0; b < 256; ++b) {
            printf("%02x:%02x%02x\t", b, left[b], right[b]);
        }
        printf("\n");
    }
    /* read compressed buffer size */
    usize = getc(infile);
    lsize = getc(infile);
    size = (usize << 8) + lsize;

    if (DEBUG)
        printf("size: %d(%02x%02x)\n", size, usize, lsize);

    /* write output, pushing pairs to stack */
    i = 0;
    while (i < size || sp) { /* more to read or stack non-empty */
        int c;
        if (sp == 0) { /* stack empty */
            c = getc(infile); /* read byte */
            if (DEBUG) printf("read byte: %02x\n", c);
            ++i;
        } else {
            c = stack[--sp]; /* pop byte */
            if (DEBUG) printf("pop byte: %02x\n", c);
        }

        if (c != left[c]) { /* pair in table */
            /* push pair */
            stack[sp++] = right[c];
            stack[sp++] = left[c];
            if (DEBUG) printf("push pair %02x%02x\n", left[c], right[c]);
        } else { /* pair not in table */
            putc(c, outfile); /* write literal byte */
            if (DEBUG) printf("write byte %02x\n", c);
        }
    }

    return 1; /* try another block */
}

int main(int argc, char* argv[])
{
    FILE* infile, * outfile;
    int notdone;

    if (argc != 3) {
        printf("Usage: expand infile outfile\n");
        return -1;
    }

    infile  = fopen(argv[1], "r");
    outfile = fopen(argv[2], "w");

    if (infile == NULL) {
        printf("bad infile\n");
        return -1;
    }

    if (outfile == NULL) {
        printf("bad outfile\n");
        return -1;
    }


    /* process blocks */
    notdone = 1;
    while (notdone)
        notdone = expand(infile, outfile);

    fclose(infile);
    fclose(outfile);

    return 0;
}
