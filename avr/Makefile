CC:=avr-gcc
FCPU:=8090000 # measured
CFLAGS:=-Os -g -Wall -mmcu=attiny2313 -DF_CPU=$(FCPU)
ADUDEFLAGS:=-p t2313 -c usbtiny

all: solar.hex

solar.elf: solar.o uart.o
	$(CC) $(CFLAGS) -o $@ $^

%.hex: %.elf
	avr-objcopy -O ihex $^ $@

%.prog: %.hex
	avrdude $(ADUDEFLAGS) -U flash:w:$^

check: solar.elf
	./check.py $^

clean:
	rm -f *.o *.hex *.elf
