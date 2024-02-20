obj-m += ouichefs.o
ouichefs-objs := fs.o super.o inode.o file.o dir.o policy.o eviction.o 

KERNELDIR ?= ~/Studium/Semester_9/Linux_Kernel_Programming/kernel/linux-6.5.7

all:
	make -C $(KERNELDIR) M=$(PWD) modules

debug:
	make -C $(KERNELDIR) M=$(PWD) ccflags-y+="-DDEBUG -g" modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -rf *~

.PHONY: all clean
