
.PHONY: all
all: bochs

%.bin: %.asm
	nasm -f bin $< -o $@

master.img: boot.bin loader.bin
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat master.img
	dd if=boot.bin of=master.img bs=512 count=1 conv=notrunc
	dd if=loader.bin of=master.img bs=512 count=4 seek=2 conv=notrunc

.PHONY: clean
clean:
	rm -rf *.bin *.img

.PHONY: bochs 
bochs: master.img
	bochs -q