/*  Byte Pair Encoding compression
    Based on idea and code from Philip Gage
    http://www.pennelynn.com/Documents/CUJ/HTML/94HTML/19940045.HTM

    Pseudocode:

    While not end of file
       Read next block of data into buffer and enter all pairs in hash table
       While compression possible (unused bytes)
          Find most frequent byte pair, break if not frequent enough
          Replace pair with an unused byte
          Add pair to pair table
       End while
       Write pair table and packed data
    End while

    Compile: 
      gcc -O3 -std=c89 -pedantic -Wall -Wextra -DDEBUG -o compress compress.c
    Usage: 
      ./compress < test/sample.txt > test/sample.bpe 2> test/compress.log
*/
#include <stdio.h>
#include <assert.h>
#include <limits.h>

#define BLOCKSIZE 5000 /* Maximum block size */
#define MAXCHARS   200 /* Charset per block (leave some unused) */
#define MINPAIRS     3 /* Min pairs needed for compress */

#ifdef DEBUG
/* C99: DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__) */
    #define DEBUG_PRINT(x) fprintf x 
#else
    #define DEBUG_PRINT(x) do {} while (0)
#endif

typedef unsigned char uchar;

/* file-wide globals per-block */
uchar buffer[BLOCKSIZE];
uchar lpair[UCHAR_MAX+1], rpair[UCHAR_MAX+1]; /* pair table */
uchar count[UCHAR_MAX+1][UCHAR_MAX+1];       /* pair counts */
unsigned short size;

void print_buffer() 
{
    int i;
    DEBUG_PRINT((stderr, "buffer[%d]:\n", size));
    for (i = 0; i < size; ++i) {
        DEBUG_PRINT((stderr, "%02x ", buffer[i]));
        if (i % 16 == 7)  DEBUG_PRINT((stderr, " "));
        if (i % 16 == 15) DEBUG_PRINT((stderr, "\n"));
    }    
    DEBUG_PRINT((stderr, "\n"));
}

void print_count()
{
    int i, j;
    DEBUG_PRINT((stderr, "(non-zero) count table:\n"));
    for (i = 0; i <= UCHAR_MAX; ++i)
        for (j = 0; j <= UCHAR_MAX; ++j)
            if (count[i][j])
                DEBUG_PRINT((stderr, "%02x%02x:%02x\t", i, j, count[i][j]));
}

/* read block from file into pair count */
/* return true if not done reading file */
int readblock(FILE* infile)
{
    int i, j, c;
    int used = 0;

    DEBUG_PRINT((stderr, "*** READ BLOCK ***\n"));

    /* reset counts and pair table */
    for (i = 0; i <= UCHAR_MAX; ++i) {
        for (j = 0; j <= UCHAR_MAX; ++j)
            count[i][j] = 0;

        lpair[i] = i;
        rpair[i] = 0;
    }

    size = 0; /* block size */

    /* C I/O, get one char at a time */
    /* stopping at EOF, BLOCKSIZE limit, or MAXCHARS limit */
    while (size < BLOCKSIZE && used < MAXCHARS) {
        /* only read now instead of in while condition */
        c = getc(infile);
        if (c == EOF) break;

        if (size > 0) {
            uchar lastc = buffer[size - 1];
            /* count pairs without overflow */
            if (count[lastc][c] < UCHAR_MAX)
                ++count[lastc][c];
        }

        /* increase used count if new, mark in pair table as used */
        if (rpair[c] == 0) {
            rpair[c] = 1;
            ++used;
        }

        buffer[size++] = c;  /* push c to buffer */
    }

    DEBUG_PRINT((stderr, "size: %d used: %d\n", size, used));
    print_buffer();
    print_count();
    DEBUG_PRINT((stderr, "\n"));
    
    return (c != EOF);
}

/* for block, write pair and packed data */
void compress()
{
    int pass, i, j, unused;

    DEBUG_PRINT((stderr, "*** COMPRESS BLOCK ***\n"));

    /* while compression possible:
       pick pairs until no unused bytes or no good pairs */
    for (pass = 1; ; ++pass) {
        int r = 0, w = 0; /* read and write index */
        uchar bestcount = 0;
        uchar bestl = 0, bestr = 0;

        DEBUG_PRINT((stderr, "COMPRESSION PASS %d\n", pass));



        for (i = 0; i <= UCHAR_MAX; ++i) {
            for (j = 0; j <= UCHAR_MAX; ++j) {
                /* record best pair and count */
                if (count[i][j] > bestcount) {
                    bestcount = count[i][j];
                    bestl = i;
                    bestr = j;
                }
            }
        }

        
        DEBUG_PRINT((stderr, "best pair %02x%02x:%d\n", 
                     bestl, bestr, bestcount));

        if (bestcount < MINPAIRS)
            break;


        /* find unused byte to use */
        for (unused = UCHAR_MAX; unused >= 0; --unused)
            if (lpair[unused] == unused && rpair[unused] == 0)
                break;

        if (unused < 0) break;  /* no more unused */

        DEBUG_PRINT((stderr, "unused byte: %02x\n", unused));


        /* replace pairs with unused byte in-place in buffer */
        while (r < size) {
            /* match best pair */
            if (r + 1 < size &&
                    buffer[r] == bestl && buffer[r + 1] == bestr) {
                buffer[w++] = unused; /* write new byte */
                r += 2; /* move read index past pair */
            } else {
                /* copy buffer[r] to buffer[w], increment indexes */
                buffer[w++] = buffer[r++];
            }
        }

        size = w; /* adjust written buffer size */

        /* TODO: update counts during writing instead */
        /* recreate count table */

        for (i = 0; i <= UCHAR_MAX; ++i)
            for (j = 0; j <= UCHAR_MAX; ++j)
                count[i][j] = 0;

        for (i = 0; i < size; ++i) {
            if (i + 1 < size) {
                uchar c = buffer[i];
                uchar d = buffer[i + 1];

                if (count[c][d] < UCHAR_MAX)
                    ++count[c][d];
            }
        }

        /* add  pair in pair table */
        lpair[unused] = bestl;
        rpair[unused] = bestr;

        print_buffer();
 
        DEBUG_PRINT((stderr, "used pair table:\n"));

        for (i = 0; i <= UCHAR_MAX; ++i) {
            if (i != lpair[i])
                DEBUG_PRINT((stderr, "%02x:%02x%02x\n", i, lpair[i], rpair[i]));
        }
        DEBUG_PRINT((stderr, "\n"));
        
    }

    DEBUG_PRINT((stderr, "\n"));
}

/* write pair table and compressed data */
void writeblock(FILE* outfile)
{
    int c = 0;

    DEBUG_PRINT((stderr, "*** WRITE BLOCK ***\n"));

    while (c <= UCHAR_MAX) {
        signed char count = 0;
        DEBUG_PRINT((stderr, "c: %02x\t", c));

        /* run of non-pairs */
        if (c == lpair[c]) {
            while (c == lpair[c] && c <= UCHAR_MAX && count > CHAR_MIN) {
                ++c;
                --count;
            }
            /* output count as negative byte */
            assert(count < 0);
            putc(count, outfile);
            DEBUG_PRINT((stderr, "count:%d\t", count));

            /* output single pair if not end of table */
            if (c <= UCHAR_MAX) {
                putc(lpair[c], outfile);
                putc(rpair[c], outfile);
                DEBUG_PRINT((stderr, "single pair %02x%02x\n", 
                             lpair[c], rpair[c]));
                ++c;
            }

        } else {
            /* run of pairs */
            int b = c; /* index of start of run */
            while (c != lpair[c] && c <= UCHAR_MAX && count < CHAR_MAX) {
                ++c;
                ++count;
            }

            /* output count */
            assert(count > 0);
            putc(count, outfile);
            DEBUG_PRINT((stderr, "count:%d\n", count));

            for (; b < c; ++b) {
                putc(lpair[b], outfile);
                putc(rpair[b], outfile);
                DEBUG_PRINT((stderr, "%02x%02x\n", lpair[b], rpair[b]));
            }


        }
    }

    /* write compressed buffer size */
    putc(size >> 8, outfile);
    putc(size & 0xFF, outfile);

    DEBUG_PRINT((stderr, "compressed size: %d (%04x)\n", size, size));


    /* write compressed buffer */
    fwrite(buffer, 1, size, outfile);
    DEBUG_PRINT((stderr, "write buffer(%d)\n", size));

}

int main(void)
{
    int notdone;
    /* process blocks */
    do {
        notdone = readblock(stdin);
        compress();
        writeblock(stdout);
    } while (notdone);

    return 0;
}
