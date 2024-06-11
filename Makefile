obj-m += gpio_device.o

all: module dt
	echo Building KM and DT
module:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
dt: gpoverlay.dts
	dtc -@ -I dts -O dtb -o gpoverlay.dtbo gpoverlay.dts
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
