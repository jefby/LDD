KVERS = $(shell uname -r)

obj-m += export_symbol.o

build:module_build
module_build:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
