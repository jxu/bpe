#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define BLOCKSIZE 5000

unsigned char  left[256];
unsigned char right[256];
unsigned char stack[256]; // overflow?


// expand blocks
// return true if not done
int expand(FILE* infile, FILE* outfile) 
{
    
    int c, i, sp = 0;
    signed char count;
    int b = 0;

    // reset pair table to defaults
    for (i = 0; i < 256; ++i)
    {
        left[i] = i;
        right[i] = 0;
    }


    // read pairs
    while(b < 256) // b = current table index
    {
        c = getc(infile);
        if (c == EOF) return 0;
        count = (signed char)c; 

        printf("b: %d Count: %d\n", b, count);

        assert(count != 0);

        // negative count: skip forward then read a pair
        if (count < 0)
        {
            b += -count;
            // doesn't handle if file unexpectedly ends
            left[b] = getc(infile);
            right[b] = getc(infile);

            // if not end table, read single pair
            if (b < 256)
            {
                printf("Read single pair %02x%02x\n", left[b], right[b]);
                ++b; // increment index to next pair
            }
        }

        else // positive count: read `count` pairs
        {
            int b_end = b + count;
            for (; b < b_end; ++b)
            {
                left[b]  = getc(infile);
                right[b] = getc(infile);
                printf("Read pair %02x%02x\n", left[b], right[b]);
                
            }
        }

    }
    
    assert(b == 256); // counts valid

    printf("Pair table:\n");
    for (b = 0; b < 256; ++b) 
    {
        printf("%02x:%02x%02x\t", b, left[b], right[b]);
    }
    printf("\n");

    // read compressed buffer size
    int usize = getc(infile);
    int lsize = getc(infile);

    int size = (usize << 8) + lsize;

    printf("size: %d\n", size);

    // write output, pushing pairs to stack
    i = 0; 
    while (i < size)
    {
        if (sp == 0) // stack empty
        {
            c = getc(infile); // read byte
            printf("read byte: %02x\n", c);
            ++i; 
        }
        else
        {
            c = stack[--sp]; // pop stack
            printf("pop byte: %02x\n", c);
        }

        if (c != left[c]) // pair in table 
        {
            // push pair
            stack[sp++] = right[c];
            stack[sp++] = left[c];
            printf("push pair %02x%02x\n", left[c], right[c]);
        }
        else // pair not in table
        {
            putc(c, outfile); // write literal byte
            printf("write byte %02x\n", c);
        }
        
    }
    

    return 1; // try another block
        
}

int main(int argc, char* argv[]) 
{
    if (argc != 3) 
    {
        printf("Usage: expand infile outfile\n");
        return -1;
    }

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

    int notdone;

    do 
    {
        notdone = expand(infile, outfile);
    } while (notdone);

    fclose(infile);
    fclose(outfile);    
}
