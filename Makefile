CC = gcc
CFLAGS = -O3
AS = as
AFLAGS = -32
LD = ld
LFLAGS = -m elf_i386 --oformat binary -N -e _start -Ttext 0x10000
OBJECTS = guest.o
BINARIES = guest.bin kvm

all: $(BINARIES)
kvm: kvm.c
	$(CC) $(CFLAGS) $< -o $@ 

guest.bin: guest.o
	$(LD) $(LFLAGS) -o $@ $<

guest.o: guest.S
	$(AS) $(AFLAGS) $< -o $@

clean:
	rm $(BINARIES) $(OBJECTS)
