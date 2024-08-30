/* The hash table ended up being twice as slow as the full count array.
 * I'm leaving it here because the code is interesting. 
 */

#include <stdio.h>

#define BLOCKSIZE 4096     /* should be power of 2 */
typedef unsigned char uchar;

uchar  count[BLOCKSIZE];   /* (approx) pair counts as hash table */
uchar   lkey[BLOCKSIZE];   /* left byte key in hash table */ 
uchar   rkey[BLOCKSIZE];   /* right byte key in hash table */ 

/* simple hash function */
unsigned int hash(uchar l, uchar r) 
{
    return (33*l + r) % BLOCKSIZE;
}


/* linear probing for pair (l,r)
   search linearly for either new empty slot or existing slot
 */
unsigned int lookup(uchar l, uchar r)
{
    unsigned int i = hash(l, r); 

    fprintf(stderr, "Looking up %02x%02x... ", l, r);
    
    while (1) {
        /* new empty slot, indicated by count == 0 */
        if (count[i] == 0) {
            fprintf(stderr, "[%03x] new\n", i);
            lkey[i] = l; rkey[i] = r;
            return i;
        }

        /* existing entry in slot, indicated by count > 0 and matching key */
        if (count[i] > 0 && lkey[i] == l && rkey[i] == r) {
            fprintf(stderr, "[%03x] %02x\n", i, count[i]);
            return i;
        }
        i = (i + 1) % BLOCKSIZE; 
    }

}
