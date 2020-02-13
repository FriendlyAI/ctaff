CC = clang
CFLAGS = -Wall -O2

OBJDIR=obj
OFILES = ${OBJDIR}/fft.o ${OBJDIR}/kiss_fft.o ${OBJDIR}/kiss_fftr.o

SRCFILES = fft.c lib/kissfft/kiss_fft.c lib/kissfft/kiss_fftr.c

build: $(OFILES)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRCFILES) -o bin/ctaff
	@echo "Compiled ctaff"

${OBJDIR}/fft.o: fft.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c fft.c -o $(OBJDIR)/fft.o

${OBJDIR}/kiss_fft.o: lib/kissfft/kiss_fft.c
	$(CC) $(CFLAGS) -c lib/kissfft/kiss_fft.c -o $(OBJDIR)/kiss_fft.o

${OBJDIR}/kiss_fftr.o: lib/kissfft/kiss_fftr.c
	$(CC) $(CFLAGS) -c lib/kissfft/kiss_fftr.c -o $(OBJDIR)/kiss_fftr.o
	
clean:
	-rm bin/beatmapper obj/*.o