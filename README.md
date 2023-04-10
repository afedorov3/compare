# Compare

Simple binary files compare utility.  

compare.exe -h:
```
Compare v2.1 [x64]
Usage: compare [opts] file1 file2 [> Output_File]
options:
  -f: First file offset(dec, 0xNUM = hex, 0NUM = oct).
  -l: Length to compare(dec, 0xNUM = hex, 0NUM = oct).
  -m: Print differences map (very slow, in case of
      output redirection make very big files).
  -q: don't print anything
  -p: don't print completion percent
  -s: Second file offset(dec, 0xNUM = hex, 0NUM = oct).
return values:
  0: files are identical
  1: fragments are identical
  2: files/fragments are different
  3: error occured
```

Example usages:
```
> compare sample1 sample2
Comparing files:
  sample1,
  size 114.95MiB (120531935 bytes), offset 0x0 (0 bytes)
and
  sample2,
  size 114.95MiB (120531935 bytes), offset 0x0 (0 bytes)
100.0%
Files are identical
time spent: 0.187s, average speed 614.696MiB/s
```

```
> compare sample1 sample3
Comparing files:
  sample1,
  size 114.95MiB (120531935 bytes), offset 0x0 (0 bytes)
and
  sample3,
  size 114.95MiB (120531920 bytes), offset 0x0 (0 bytes)
Files have different size, comparing first 120531920 bytes.
100.0%
Found 1 difference at 0x40A4087.
time spent: 0.183s, average speed 628.132MiB/s
```
