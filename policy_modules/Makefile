KERNELDIR_LKP ?= ../../linux-6.5.7

obj-m += largest_file_policy.o
CFLAGS_largest_file_policy.o := -DDEBUG
# MY_CFLAGS += -g -DDEBUG
# ccflags-y += ${MY_CFLAGS}
# CC += ${MY_CFLAGS}

all :
	make -C $(KERNELDIR_LKP) M=$$PWD modules KBUILD_EXTRA_SYMBOLS=/home/saschathiemann/Studium/Semester_9/Linux_Kernel_Programming/kernel/lkp_project/Module.symvers

# debug:
# 	make -C $(KERNELDIR_LKP) M=$(PWD) modules 
# 	EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C $(KERNELDIR_LKP) M=$$PWD clean 
