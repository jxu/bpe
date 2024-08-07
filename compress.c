



#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

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
int size;

// read block from file into pair count
// return if done reading file
bool readblock(FILE* infile)
{
    // clear count and reset pair table
    memset(count, 0, 256 * 256);

    for (int i=0; i<256; ++i) {
        left[i] = i;
        right[i] = 0;
    }


    size = 0; // block size
    int used = 0;

    int c;

    // C I/O, get one char at a time
    // stopping at EOF, BLOCKSIZE limit, or MAXCHARS limit
    while ((c = getc(infile)) != EOF 
           && size < BLOCKSIZE && used < MAXCHARS) {

        // count pair
        if (size > 0) {
            uchar lastc = buffer[size-1];

            if (count[lastc][c] < 255)
                ++count[lastc][c];
        }

        // increase used count if new, mark as used
        if (right[c] == 0) {
            right[c] = 1;
            ++used;
        }

        buffer[size] = c;  // write char at size


        ++size;
    }



    printf("size: %d used: %d\n", size, used);

    printf("buffer:\n");
    for (int i=0; i<size; ++i)
        printf("%02x ", buffer[i]);

    printf("\n(non-zero) count table:\n");
    for (int i=0; i<256; ++i) {
        for (int j=0; j<256; ++j) {
            if (count[i][j]) 
                printf("%02x%02x:%02x\n", i, j, count[i][j]);
        }
    }
    

    printf("\npair table:\n");
    for (int i=0x60; i<0x68; ++i) {
        printf("%02x: %02x %02x\n", i, left[i], right[i]);
    }

    printf("\n\n");


    return (c == EOF);
}

// for block, write pair and packed data
void compress() {
    // while compression possible

    int unused = 0;
    // count unused from right array
    for (int i=0; i<256; ++i) {
        if (right[i] == 0) ++unused;
    }

    // pick pairs until no unused bytes or no good pairs
    while (unused > 0) {

    
        uchar bestcount = 0;
        uchar bestleft, bestright;
        for (int i=0; i<256; ++i) {
            for (int j=0; j<256; ++j) {
                // record best pair and count
                if (count[i][j] > bestcount) {
                    bestcount = count[i][j];
                    bestleft = i; bestright = j;    
                }
            }
        }
        
        printf("best pair %02x%02x:%d\n", bestleft, bestright, bestcount);

        if (bestcount < MINPAIRS) 
            break;


            

        // find unused byte to use
        int y = 0;
        for (int i=0; i<256; ++i) {
            if (i == left[i] && right[i] == 0) {
                y = i;
                break;
            }
        }

        printf("unused byte: %02x\n", y);
        

        int r = 0; // read index
        int w = 0; // write index
         

        // replace pairs with unused byte in-place in buffer
        while (r < size) {

            // match best pair
            if (r+1 < size && 
                buffer[r] == bestleft && buffer[r+1] == bestright) {

                buffer[w] = y; // write new byte

                ++w;
                r += 2; // move read index past pair


            }
            
            else {
                buffer[w] = buffer[r];
                ++w; ++r;
            }

            
        }

        size = w; // adjust written buffer size
        
        // TODO: update counts during writing instead
        // recreate count table
        memset(count, 0, 256 * 256);
        for (int i=0; i<size; ++i) {
            if (i+1 < size) {
                uchar c = buffer[i];
                uchar d = buffer[i+1];

                if (count[c][d] < 255) {
                    ++count[c][d];
                }
            }
        }

        --unused;

        printf("new buffer: ");
        for (int i=0; i<size; ++i) {
            printf("%02x ", buffer[i] );
        }
        printf("\n");

        printf("new counts:\n");
        for (int i=0; i<256; ++i) {
            for (int j=0; j<256; ++j)
                if (count[i][j]) 
                    printf("%02x%02x:%d\n", i, j, count[i][j]);
        }       


        // add  pair in pair table
        left[y] = bestleft;
        right[y] = bestright;


        
        printf("\npair table:\n");
        for (int i=0; i<256; ++i) {
            printf("%02x: %02x %02x\n", i, left[i], right[i]);
        }

        break; // temp
    }
}



int main(int argc, char* argv[]) {

    if (argc != 3) {
        printf("Usage: compress infile outfile\n");
        return -1;
    }

    // TODO: file i/o checking
    FILE* infile = fopen(argv[1], "rb");    
    readblock(infile);
    compress();
}













