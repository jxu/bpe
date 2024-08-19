/* Byte Pair Encoding compression
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
uchar left[UCHAR_MAX+1], right[UCHAR_MAX+1]; /* pair table */
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

        left[i] = i;
        right[i] = 0;
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
        if (right[c] == 0) {
            right[c] = 1;
            ++used;
        }

        buffer[size++] = c;  /* push c to buffer */
    }

    DEBUG_PRINT((stderr, "size: %d used: %d\n", size, used));

    print_buffer();
    
    DEBUG_PRINT((stderr, "(non-zero) count table:\n"));
    for (i = 0; i <= UCHAR_MAX; ++i)
        for (j = 0; j <= UCHAR_MAX; ++j)
            if (count[i][j])
                DEBUG_PRINT((stderr, "%02x%02x:%02x\t", i, j, count[i][j]));

    DEBUG_PRINT((stderr, "\n"));
    

    return (c != EOF);
}

/* for block, write pair and packed data */
void compress()
{
    int pass, i, j, y;

    DEBUG_PRINT((stderr, "*** COMPRESS BLOCK ***\n"));

    /* while compression possible:
       pick pairs until no unused bytes or no good pairs */
    for (pass = 1; ; ++pass) {
        int r = 0, w = 0; /* read and write index */
        uchar bestcount = 0;
        uchar bestleft = 0, bestright = 0;

        DEBUG_PRINT((stderr, "COMPRESSION PASS %d\n", pass));

        for (i = 0; i <= UCHAR_MAX; ++i) {
            for (j = 0; j <= UCHAR_MAX; ++j) {
                /* record best pair and count */
                if (count[i][j] > bestcount) {
                    bestcount = count[i][j];
                    bestleft = i;
                    bestright = j;
                }
            }
        }

        
        DEBUG_PRINT((stderr, "best pair %02x%02x:%d\n", 
                     bestleft, bestright, bestcount));

        if (bestcount < MINPAIRS)
            break;


        /* find unused byte to use */
        for (y = UCHAR_MAX; y >= 0; --y)
            if (left[y] == y && right[y] == 0)
                break;

        if (y < 0) break;  /* no more unused */

        DEBUG_PRINT((stderr, "unused byte: %02x\n", y));


        /* replace pairs with unused byte in-place in buffer */
        while (r < size) {
            /* match best pair */
            if (r + 1 < size &&
                    buffer[r] == bestleft && buffer[r + 1] == bestright) {
                buffer[w++] = y; /* write new byte */
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
        left[y] = bestleft;
        right[y] = bestright;

        print_buffer();
 
        DEBUG_PRINT((stderr, "used pair table:\n"));

        for (i = 0; i <= UCHAR_MAX; ++i) {
            if (i != left[i])
                DEBUG_PRINT((stderr, "%02x:%02x%02x\n", i, left[i], right[i]));
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
        if (c == left[c]) {
            while (c == left[c] && c <= UCHAR_MAX && count > CHAR_MIN) {
                ++c;
                --count;
            }
            /* output count as negative byte */
            assert(count < 0);
            putc(count, outfile);
            DEBUG_PRINT((stderr, "count:%d\t", count));

            /* output single pair if not end of table */
            if (c <= UCHAR_MAX) {
                putc(left[c], outfile);
                putc(right[c], outfile);
                DEBUG_PRINT((stderr, "single pair %02x%02x\n", 
                             left[c], right[c]));
                ++c;
            }

        } else {
            /* run of pairs */
            int b = c; /* index of start of run */
            while (c != left[c] && c <= UCHAR_MAX && count < CHAR_MAX) {
                ++c;
                ++count;
            }

            /* output count */
            assert(count > 0);
            putc(count, outfile);
            DEBUG_PRINT((stderr, "count:%d\n", count));

            for (; b < c; ++b) {
                putc(left[b], outfile);
                putc(right[b], outfile);
                DEBUG_PRINT((stderr, "%02x%02x\n", left[b], right[b]));
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



int main(int argc, char* argv[])
{
    int notdone;
    FILE* infile, * outfile;

    if (argc != 3) {
        DEBUG_PRINT((stderr, "Usage: compress infile outfile\n"));
        return -1;
    }

    infile  = fopen(argv[1], "r");
    outfile = fopen(argv[2], "w");

    if (infile == NULL) {
        DEBUG_PRINT((stderr, "bad infile\n"));
        return -1;
    }

    if (outfile == NULL) {
        DEBUG_PRINT((stderr, "bad outfile\n"));
        return -1;
    }

    notdone = 1;
    while (notdone) {
        notdone = readblock(infile);
        compress();
        writeblock(outfile);
    }

    fclose(infile);
    fclose(outfile);

    return 0;
}
