# ctaff
An automated rhythm game map generator written in pure C.

### Setup
Run `make` in `ctaff/` subfolder.

### Usage
Run `ctaff/bin/ctaff -i "<INPUT FILEPATH> -o <OUTPUT FILEPATH>"` or `ctaff/bin/ctaff` if a raw PCM file is already in `tmp/tmp.raw`. The PCM file must be in mono 32-bit float little-endian format.

### Dependencies
[KISS FFT](https://github.com/mborgerding/kissfft)
- Required files are included in `lib/kissfft`