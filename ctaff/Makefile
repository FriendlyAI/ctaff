CC = clang
CFLAGS = -Wall -O2

OBJDIR=obj
OFILES = ${OBJDIR}/ctaff.o ${OBJDIR}/kiss_fft.o ${OBJDIR}/kiss_fftr.o

SRCFILES = ctaff.c lib/kissfft/kiss_fft.c lib/kissfft/kiss_fftr.c

build: $(OFILES)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRCFILES) -o bin/ctaff
	@echo "Compiled ctaff"

${OBJDIR}/ctaff.o: ctaff.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c ctaff.c -o $(OBJDIR)/ctaff.o

${OBJDIR}/kiss_fft.o: lib/kissfft/kiss_fft.c
	$(CC) $(CFLAGS) -c lib/kissfft/kiss_fft.c -o $(OBJDIR)/kiss_fft.o

${OBJDIR}/kiss_fftr.o: lib/kissfft/kiss_fftr.c
	$(CC) $(CFLAGS) -c lib/kissfft/kiss_fftr.c -o $(OBJDIR)/kiss_fftr.o
	
clean:
	-rm bin/ctaff obj/*.o