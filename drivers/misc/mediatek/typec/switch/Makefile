# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/usb/typec

ifeq ($(CONFIG_MTK_USB_TYPEC_U3_MUX), y)
obj-y += mux_switch.o
obj-y += fusb304.o
obj-y += ptn36241g.o
obj-y += ps5169.o
endif

#prize-add-pengzhipeng-20220514-start
obj-y += ptn36043a.o
#prize-add-pengzhipeng-20220514-end
ifeq ($(CONFIG_OCP96011_I2C), y)
obj-y += ocp96011-i2c.o
endif