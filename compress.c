/* Byte Pair Encoding (BPE) Compression
   Based on idea and code from Philip Gage
   http://www.pennelynn.com/Documents/CUJ/HTML/94HTML/19940045.HTM

   Pseudocode
   For each block:
       Read block of data into buffer, until max size or no more unused bytes 
       For each compression pass:
           Count unused bytes and most frequent pair
           Break if no unused bytes or no frequent enough pair
           Replace pair with an unused byte (in single buffer)
           Add pair to pair table

       Write pair table and packed data

   Compile (with DEBUG): 
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
    #define DEBUG_PRINT(x) (void)0  /* suppress GCC warning */
#endif

typedef unsigned char uchar;

/* file-wide globals per-block */
uchar buffer[BLOCKSIZE];
uchar lpair[UCHAR_MAX+1];               /* left pair table*/
uchar rpair[UCHAR_MAX+1];               /* right pair table */
uchar count[UCHAR_MAX+1][UCHAR_MAX+1];  /* pair counts */
unsigned short size;                    /* block size (short to test 16-bit) */

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

/* print unused pairs in pair table */
void print_pairs() 
{
    int i;
    DEBUG_PRINT((stderr, "used pair table:\n"));

    for (i = 0; i <= UCHAR_MAX; ++i) {
        if (i != lpair[i])
            DEBUG_PRINT((stderr, "%02x:%02x%02x\n", i, lpair[i], rpair[i]));
    }    
}

/* read block from file into pair count */
/* return true if not done reading file */
int readblock(FILE* infile)
{
    int i, c;
    int used = 0;

    DEBUG_PRINT((stderr, "*** READ BLOCK ***\n"));

    /* reset pair table */
    for (i = 0; i <= UCHAR_MAX; ++i) {
        lpair[i] = i;
        rpair[i] = 0;
    }

    size = 0; /* block size */

    /* C I/O, get one char at a time */
    /* stopping at EOF, BLOCKSIZE limit, or MAXCHARS limit */
    while (size < BLOCKSIZE && used < MAXCHARS) {
        /* only read now instead of in while condition to avoid over-reading */
        c = getc(infile);
        if (c == EOF) break;

        /* if unused, mark byte as used and increase used count */
        if (rpair[c] == 0) {
            rpair[c] = 1;
            ++used;
        }

        buffer[size++] = c;  /* push c to buffer */
    }

    DEBUG_PRINT((stderr, "size: %d used: %d\n", size, used));
    print_buffer();
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

        /* clear count table */
        for (i = 0; i <= UCHAR_MAX; ++i)
            for (j = 0; j <= UCHAR_MAX; ++j)
                count[i][j] = 0;

        /* recreate count table, tracking best pair and count */
        for (i = 0; i+1 < size; ++i) {
            uchar a = buffer[i], b = buffer[i+1];
            if (count[a][b] < UCHAR_MAX) {
                ++count[a][b];
                if (count[a][b] > bestcount) {
                    bestcount = count[a][b];
                    bestl = a; bestr = b;
                }
            }
        }        
        
        DEBUG_PRINT((stderr, "best pair %02x%02x:%02x\n", 
                     bestl, bestr, bestcount));

        if (bestcount < MINPAIRS)
            break;

        /* find unused byte to use from end */
        for (unused = UCHAR_MAX; unused >= 0; --unused)
            if (lpair[unused] == unused && rpair[unused] == 0)
                break;

        if (unused < 0) {
            DEBUG_PRINT((stderr, "no more unused bytes\n"));
            break; 
        }
        
        DEBUG_PRINT((stderr, "unused byte: %02x\n", unused));


        /* replace pairs with unused byte in-place in buffer */
        while (r < size) {
            /* match best pair */
            if (r+1 < size && buffer[r] == bestl && buffer[r+1] == bestr) {
                buffer[w++] = unused; /* write new byte */
                r += 2; /* move read index past pair */
            } else {
                /* copy buffer[r] to earlier buffer[w], increment indexes */
                buffer[w++] = buffer[r++];
            }
        }

        size = w; /* adjust written buffer size */


        /* add  pair in pair table */
        lpair[unused] = bestl;
        rpair[unused] = bestr;

        print_buffer();
        print_pairs();
        
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
        DEBUG_PRINT((stderr, "c=%02x ", c));

        /* run of non-pairs */
        if (c == lpair[c]) {
            while (c == lpair[c] && c <= UCHAR_MAX && count > CHAR_MIN) {
                ++c;
                --count;
            }
            /* output count as negative byte */
            assert(count < 0);
            putc(count, outfile);
            DEBUG_PRINT((stderr, "count:%d\n", count));

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
    DEBUG_PRINT((stderr, "write buffer(%d)\n\n", size));

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
