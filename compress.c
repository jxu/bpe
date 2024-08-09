
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#define BLOCKSIZE 5000 // Maximum block size
#define MAXCHARS   200 // Charset per block (leave some unused)
#define MINPAIRS     2 // Min pairs needed for compress 

typedef unsigned char uchar;
typedef unsigned int uint;

// globals
uchar buffer[BLOCKSIZE];
uchar left[256];
uchar right[256];
uchar count[256][256];  // pair counts (up to 255)   
int size, used;  // per block

// read block from file into pair count
// return true if not done reading file
bool readblock(FILE* infile)
{
    printf("*** READ BLOCK ***\n");

    // clear count and reset pair table
    memset(count, 0, 256 * 256);

    for (int i=0; i<256; ++i) 
    {
        left[i] = i;
        right[i] = 0;
    }

    size = 0; // block size
    used = 0;

    int c;

    // C I/O, get one char at a time
    // stopping at EOF, BLOCKSIZE limit, or MAXCHARS limit
    while ((c = getc(infile)) != EOF 
           && size < BLOCKSIZE && used < MAXCHARS) 
    {

        // count pair
        if (size > 0) 
        {
            uchar lastc = buffer[size-1];

            if (count[lastc][c] < 255)
                ++count[lastc][c];
        }

        // increase used count if new, mark in pair table as used
        if (right[c] == 0) 
        {
            right[c] = 1;
            ++used;
        }

        buffer[size] = c;  // write char at index `size`


        ++size;
    }



    printf("size: %d used: %d\n", size, used);

    printf("buffer:\n");
    for (int i=0; i<size; ++i)
        printf("%02x ", buffer[i]);

    
    printf("\n(non-zero) count table:\n");
    for (int i=0; i<256; ++i) 
    {
        for (int j=0; j<256; ++j) 
        {
            if (count[i][j]) 
                printf("%02x%02x:%02x\t", i, j, count[i][j]);
        }
    }
    
    
    /*
    printf("\npair table:\n");
    for (int i=0x60; i<0x68; ++i) {
        printf("%02x: %02x %02x\n", i, left[i], right[i]);
    }
    */

    printf("\n");


    return (c != EOF);
}

// for block, write pair and packed data
void compress() 
{
    printf("*** COMPRESS BLOCK ***\n");

    int pass = 1;

    // while compression possible:
    // pick pairs until no unused bytes or no good pairs
    while (1) 
    {
        printf("COMPRESSION PASS %d\n", pass);
    
        uchar bestcount = 0;
        uchar bestleft=0, bestright=0;
        
        for (int i=0; i<256; ++i) 
        {
            for (int j=0; j<256; ++j) 
            {
                // record best pair and count
                if (count[i][j] > bestcount) 
                {
                    bestcount = count[i][j];
                    bestleft = i; bestright = j;    
                }
            }
        }


        printf("best pair %02x%02x:%d\n", bestleft, bestright, bestcount);

        if (bestcount < MINPAIRS) 
            break;


        // find unused byte to use
        int y;
        for (y=255; y>=0;--y)
            if (left[y] == y && right[y] == 0) 
                break;
        

        if (y < 0) break;  // no more unused 

        printf("unused byte: %02x\n", y);
        

        int r = 0; // read index
        int w = 0; // write index
         

        // replace pairs with unused byte in-place in buffer
        while (r < size) 
        {
            // match best pair
            if (r+1 < size && 
                buffer[r] == bestleft && buffer[r+1] == bestright) 
            {
                buffer[w] = y; // write new byte

                ++w;
                r += 2; // move read index past pair
            } 
            else 
            {
                buffer[w] = buffer[r];
                ++w; ++r;
            }
        }

        size = w; // adjust written buffer size
        
        // TODO: update counts during writing instead
        // recreate count table
        memset(count, 0, 256 * 256);
        for (int i=0; i<size; ++i) 
        {
            if (i+1 < size) 
            {
                uchar c = buffer[i];
                uchar d = buffer[i+1];

                if (count[c][d] < 255) 
                {
                    ++count[c][d];
                }
            }
        }

        ++pass;

        
        printf("new buffer(%d): ", size);
        for (int i=0; i<size; ++i) 
        {
            printf("%02x ", buffer[i] );
        }
        printf("\n");

        /*
        printf("new counts:\n");
        for (int i=0; i<256; ++i) {
            for (int j=0; j<256; ++j)
                if (count[i][j]) 
                    printf("%02x%02x:%d\t", i, j, count[i][j]);
        } 
        */      


        // add  pair in pair table
        left[y] = bestleft;
        right[y] = bestright;


        
        printf("\nused pair table:\n");

        for (int i=0; i<256; ++i) 
        {
            if (i != left[i]) 
                printf("%02x:%02x%02x\n", i, left[i], right[i]);
        }

        printf("\n");
    }

    printf("\n");
}

// write pair table and compressed data
void writeblock(FILE* outfile) 
{
    printf("*** WRITE BLOCK ***\n");

    int c = 0;
    signed char count = 0;

    while (c < 256) 
    {
        printf("c: %02x\n",c);
    
        count = 0;
        // run of non-pairs
        if (c == left[c]) 
        {
            while (c == left[c] && c < 256 && count > -128) 
            {
                ++c;
                --count;
            }
            // output count as negative byte
            assert(count < 0);
            putc(count, outfile);
            printf("count:%d\n", count);

            // output single pair if not end of table
            if (c < 256) 
            {
                putc(left[c], outfile);
                putc(right[c], outfile); 
                printf("single pair %02x%02x\n", left[c], right[c]);
                ++c;
            }
            
        } 
        else 
        {
            // run of pairs
            int b = c; // index of start of run
            while (c != left[c] && c < 256 && count < 127) 
            {
                ++c;
                ++count;
            }

            // output count
            assert(count > 0);
            putc(count, outfile);
            printf("count:%d\n", count);

            for (; b < c; ++b) 
            {
                putc(left[b], outfile);
                putc(right[b], outfile);
                printf("%02x%02x\n", left[b], right[b]);
            }

            
        }
    }

    // write compressed buffer size
    assert(size <= 0xFFFF);
    putc(size >> 8, outfile);
    putc(size & 0xFF, outfile);

    printf("compressed size: %d (%04x)\n", size, size);
    

    // write compressed buffer
    fwrite(buffer, 1, size, outfile);
    printf("write buffer\n");
    
}



int main(int argc, char* argv[]) 
{

    if (argc != 3) 
    {
        printf("Usage: compress infile outfile\n");
        return -1;
    }

    // TODO: file i/o checking
    // TODO: loop through blocks
    FILE* infile  = fopen(argv[1], "r");
    FILE* outfile = fopen(argv[2], "w"); 

    if (infile == NULL) 
    {
        printf("bad infile\n");
        return -1;
    }

    if (outfile == NULL) 
    {
        printf("bad outfile\n");
        return -1;
    }

    bool notdone;
    do 
    {
        notdone = readblock(infile);
        compress();
        writeblock(outfile);
    } while (notdone);
}


