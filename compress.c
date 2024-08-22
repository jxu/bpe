/*  Byte Pair Encoding compression
    Based on idea and code from Philip Gage

    Pseudocode:

    While not end of file
       Read next block of data into buffer and
          enter all pairs in hash table with counts of their occurrence
       While compression possible
          Find most frequent byte pair
          Replace pair with an unused byte
          If substitution deletes a pair from buffer,
             decrease its count in the hash table
          If substitution adds a new pair to the buffer,
             increase its count in the hash table
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
#define MAXPASS    200 /* Max number of passes per block */
#define HASHSIZE  4093 /* Hash table size (should be large prime) */

#ifdef DEBUG
/* C99: DEBUG_PRINT(...) fprintf(stderr, __VA_ARGS__) */
    #define DEBUG_PRINT(x) fprintf x 
#else
    #define DEBUG_PRINT(x) do {} while (0)
#endif

typedef unsigned char uchar;

/* file-wide globals per-block */
uchar buf[BLOCKSIZE];
uchar lpair[UCHAR_MAX+1], rpair[UCHAR_MAX+1]; /* pair table */
uchar count[UCHAR_MAX+1][UCHAR_MAX+1];       /* (approx) pair counts */
unsigned short size;

void print_buffer() 
{
    int i;
    DEBUG_PRINT((stderr, "buf[%d]:\n", size));
    for (i = 0; i < size; ++i) {
        DEBUG_PRINT((stderr, "%02x ", buf[i]));
        if (i % 16 == 7)  DEBUG_PRINT((stderr, " "));
        if (i % 16 == 15) DEBUG_PRINT((stderr, "\n"));
    }    
    DEBUG_PRINT((stderr, "\n"));
}

void print_count() 
{
    int i, j, printed = 0;
    DEBUG_PRINT((stderr, "(non-zero) count table:\n"));
    for (i = 0; i <= UCHAR_MAX; ++i)
        for (j = 0; j <= UCHAR_MAX; ++j) {            
            if (count[i][j]) {
                DEBUG_PRINT((stderr, "%02x%02x:%02x\t", i, j, count[i][j]));
                ++printed;
                if (printed % 8 == 0) {
                    DEBUG_PRINT((stderr, "\n"));
                }
            }

        }

    DEBUG_PRINT((stderr, "\n"));
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
            uchar lastc = buf[size - 1];
            if (count[lastc][c] < UCHAR_MAX) ++count[lastc][c];
        }

        /* increase used count if new, mark in pair table as used */
        if (rpair[c] == 0) {
            rpair[c] = 1;
            ++used;
        }

        buf[size++] = c;  /* push c to buf */
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
    int pass, i, j, newc;

    DEBUG_PRINT((stderr, "*** COMPRESS BLOCK ***\n"));

    /* while compression possible:
       pick pairs until no unused bytes or no good pairs */
    for (pass = 1; pass <= MAXPASS; ++pass) {
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
        for (newc = UCHAR_MAX; newc >= 0; --newc)
            if (lpair[newc] == newc && rpair[newc] == 0)
                break;

        if (newc < 0) break;  /* no more unused */

        DEBUG_PRINT((stderr, "unused byte: %02x\n", newc));


        /* Replace pairs with unused byte in-place in buffer. 
           Always r >= w so always read original before write */
        while (r < size) {
            fprintf(stderr, "r %d w %d\n", r, w);

            /* match best pair */
            if (r + 1 < size &&
                    buf[r] == bestl && buf[r + 1] == bestr) {

                /* update count based on removing this pair */
                if (count[bestl][bestr] > 0) --count[bestl][bestr];

                /* decrease count of old after-pair 
                   (not including end-of-block) */
                if (w+1 < size && count[buf[w]][buf[w+1]] > 0) {
                    --count[buf[w]][buf[w+1]];
                    fprintf(stderr, "-1 %02x%02x\n", buf[w], buf[w+1]);
                }

                /* decrease count of old before-pair */
                if (w-1 >= 0 && count[buf[w-1]][buf[w]] > 0) {
                    --count[buf[w-1]][buf[w]];
                    fprintf(stderr, "-2 %02x%02x\n", buf[w-1], buf[w]);                    
                }

                /* increase count of new after-pair */
                if (w+1 < size && count[newc][buf[w+1]] < UCHAR_MAX) {
                    ++count[newc][buf[w+1]];
                    fprintf(stderr, "+1 %02x%02x\n", newc, buf[w+1]);
                }
                
                /* increase count of new before-pair */
                if (w-1 >= 0 && count[buf[w-1]][newc] < UCHAR_MAX) {
                    ++count[buf[w-1]][newc];
                    fprintf(stderr, "+2 %02x%02x\n", buf[w-1], newc);
                }

                buf[w++] = newc; /* write new byte */
                r += 2; /* move read index past pair */
            } else {
                /* copy buf[r] to buf[w], increment indexes */
                buf[w++] = buf[r++];
            }
        }

        size = w; /* adjust written buf size */


        /* add  pair in pair table */
        lpair[newc] = bestl;
        rpair[newc] = bestr;

        print_buffer();

        print_count();
 
        DEBUG_PRINT((stderr, "used pair table:\n"));

        for (i = 0; i <= UCHAR_MAX; ++i) {
            if (i != lpair[i])
                DEBUG_PRINT((stderr, "%02x:%02x%02x\n", i, lpair[i], rpair[i]));
        }
        DEBUG_PRINT((stderr, "\n"));
        
    }

    if (pass == MAXPASS) {
        DEBUG_PRINT((stderr, "MAXPASS reached!"));
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

    /* write compressed buf size */
    putc(size >> 8, outfile);
    putc(size & 0xFF, outfile);

    DEBUG_PRINT((stderr, "compressed size: %d (%04x)\n", size, size));


    /* write compressed buf */
    fwrite(buf, 1, size, outfile);
    DEBUG_PRINT((stderr, "write buf(%d)\n", size));

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
