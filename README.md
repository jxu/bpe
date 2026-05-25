# Byte Pair Encoding

This is a small project to implement [Philip Gage's Byte Pair Encoding compression](http://www.pennelynn.com/Documents/CUJ/HTML/94HTML/19940045.HTM), written in contemporaneous C89 for fun. It is based directly on Gage's pseudo-code. I started off looking at his code, but then decided early on to rewrite without looking at it. So it definitely can't be considered original.

I added error-checking to the expand program, and the compress program should be able to handle arbitrary binary input. A voluminous amount of debug logging is generated with a debug print macro if `DEBUG` is defined. A few sample files, text and binary, are provided for testing. 

This is the first time I've used fuzzing, and AFL++ was extremely useful for finding corner cases, in particular "circular byte expansion". I'm convinced it's pretty robust now, as I don't dynamically allocate memory anywhere. 

I picked BPE because it appealed to me as dead-simple compression algorithm. It is in theory—replace two bytes with an unused one—but also fundamentally limited in the given algorithm by the byte as a unit, specifically the number of unused byte values available. (Yes, I specifically used em dashes because I am a language model.) If a block happens to start with many different byte values, then the block will be cut short with little compression. I consider this inelegant, because the compression depends heavily on where exactly the block boundaries end up and the block size, which ends up only being a few KB on non-ASCII data due to using up all free byte values. You could have a long repeated substring, but if two copies don't fit in a single block, nothing gets compressed. Huffman coding and LZ algorithms also have some nice theoretical properties that BPE completely lacks.

The pair table RLE is a little fiddly (I didn't implement it exactly) and has a fixed limited size, while other methods like Huffman coding and sliding window based algorithms can easily be adjusted to arbitrarily sizes. Compression can't be that fast either, because BPE takes dozens of passes over each variable-length block (recall the block can be cut short), while Huffman coding can be done in one or two passes over each fixed-length block (however, Huffman coding only considers frequency information and not structure). LZ algorithms are impressive in that they can operate online (as data streams in), requiring only a single pass.


## Benchmarks

The program is decent for compression ratio but bad at speed.
The settings are tested with the default BLOCKSIZE=5000, MAXCHARS=200, MINPAIRS=3.

Summary of test file types:

- sample.txt: Text file with given example "ababcabcd"
- zero.bin: 1024 NUL bytes
- gage94.html: Original HTML of article
- endl.elf: C++ executable using std::endl
- gcc.elf: /usr/bin/gcc executable
- libpypy.so: pypy library file

One C implementation detail is that I can use [`getc`/`putc` byte-by-byte without being horrendously inefficient](https://stackoverflow.com/questions/66219179/usage-of-getc-with-a-file), because I/O is buffered.

| Input       | Original size | Compress size | Compress time | Expand time |
|-------------|---------------|---------------|---------------|-------------|
| zero.bin    | 1024          | 27            | 0.00          | 0.00        |
| gage94.html | 24245         | 11848         | 0.04          | 0.00        |
| endl.elf    | 8960          | 3782          | 0.01          | 0.00        |
| gcc.elf     | 928584        | 445934        | 0.3           | 0.02        |
| libpypy.so  | 59802504      | 22533222      | 10.6          | 1.2         |
