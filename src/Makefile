
boot.bin: boot.asm
	nasm -f bin boot.asm -o boot.bin

master.img:
	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat master.img

build: boot.bin master.img
	dd if=boot.bin of=master.img bs=512 count=1 conv=notrunc

clean:
	rm -rf *.bin *.img