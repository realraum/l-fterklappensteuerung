##
##  spreadspace avr utils
##
##
##  Copyright (C) 2013-2015 Christian Pointner <equinox@spreadspace.org>
##
##  This file is part of spreadspace avr utils.
##
##  spreadspace avr utils is free software: you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation, either version 3 of the License, or
##  any later version.
##
##  spreadspace avr utils is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with spreadspace avr utils. If not, see <http://www.gnu.org/licenses/>.
##

NAME := dampercontrol
BOARD_TYPE := arduinoProMicro
CXX_OBJ := main.o settings.o comm.o pressure.o
LIBS := util led lufa-descriptor-usbserial dualusbio bmp280
CXX_LIBS := arduino-stub
EXTERNAL_LIBS := lufa pjon
SPREADAVR_PATH := ../contrib/avr-utils

PJON_PATH := $(SPREADAVR_PATH)/contrib/PJON-3.0
PJON_OPTS :=

LUFA_PATH := $(SPREADAVR_PATH)/contrib/lufa-LUFA-151115
LUFA_OPTS  = -D USB_DEVICE_ONLY
LUFA_OPTS += -D DEVICE_STATE_AS_GPIOR=0
LUFA_OPTS += -D ORDERED_EP_CONFIG
LUFA_OPTS += -D FIXED_CONTROL_ENDPOINT_SIZE=8
LUFA_OPTS += -D FIXED_NUM_CONFIGURATIONS=1
LUFA_OPTS += -D USE_FLASH_DESCRIPTORS
LUFA_OPTS += -D USE_STATIC_OPTIONS="(USB_DEVICE_OPT_FULLSPEED | USB_OPT_REG_ENABLED | USB_OPT_AUTO_PLL)"
LUFA_OPTS += -D INTERRUPT_CONTROL_ENDPOINT

LUFA_OPTS += -D USB_MANUFACTURER="L\"xro\""
LUFA_OPTS += -D USB_PRODUCT="L\"dampercontrol\""

LUFA_COMPONENTS := USB USBCLASS ADC

include $(SPREADAVR_PATH)/include.mk
## PJON likes c++11
CXXFLAGS += -std=c++11 -I$(LUFA_PATH)

## uncomment to printf floats
LDFLAGS += -Wl,-u,vfprintf -lprintf_flt -lm
AVRDUDE_PORT := $(shell $(SPREADAVR_PATH)/tools/detect_newest_acm)
#AVRDUDE_PORT := $(shell $(SPREADAVR_PATH)/tools/detect_lufa_cdc 1)
RESET_FUNC := $(SPREADAVR_PATH)/tools/reset_sparkfun
