# ctaff
An automated beat mapper and rhythm game map generator written in pure C.

### Setup
Run `make` in `src/` subfolder.

### Usage
Run `bin/ctaff -i "<INPUT FILEPATH>" -o "<OUTPUT FILEPATH>"`.

### Dependencies
- [KISS FFT](https://github.com/mborgerding/kissfft)
	- Required files are included in `lib/kissfft`

### Requirements
- [FFmpeg](https://www.ffmpeg.org/)
	- Command-line tools required

### Output
Output is a binary file of beats detected. There are no headers and each beat is 5 bytes: the first byte is an ASCII character representing the layer in which the beat was detected. The next 4 bytes are a little-endian float representing the time, in seconds, of the detection.