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