Status
=======

It works very well.

Pressure Sensor code needs to be added. Work on adding bmp180 lib to avr-utils needs to be finished.

Serial Msg Injection
====================

## Header Bytes

1. '>' start tx
2. PJON destination id. usually 1
3. Number of Bytes to follow
4. Msg Type

## Damper Control Bytes following header

MsgType = 0

5. 0
6. 0 || 1 || 2 for Danper 0
7. 0 || 1 || 2 for Danper 1
8. 0 || 1 || 2 for Danper 2
9. 0 for Fans off, 2 for Fan on, 1 for Laminafan on, 3 for all fans on

Testing: Injecting Test PJON Packets
====================================

#### Close Damper 0, Open Damers 1,2 and set FAN to On

    echo -ne ">\x01\x06\x00\x00\x00\x01\x01\x01" >| /dev/ttyACM3

#### Open Damper 0,1,2 and set FAN to On

    echo -ne ">\x01\x06\x00\x00\x01\x01\x01\x01" >| /dev/ttyACM3

#### Close all Dampers, Set Fan to ON
(not: fan won't start if all dampers closed)

    echo -ne ">\x01\x06\x00\x00\x00\x00\x00\x01" >| /dev/ttyACM3

#### Set Damper0 to Half-Open, Damper1 and 2 to Open and Fan to OFF

    echo -ne ">\x01\x06\x00\x00\x02\x01\x01\x00" >| /dev/ttyACM3

#### Set Damper0 to Half-Open, Damper1 and 2 to Open and Fan to On

    echo -ne ">\x01\x06\x00\x00\x02\x01\x01\x01" >| /dev/ttyACM3

#### Set damper-open-position to 80 for damper 0,1 and for damper 2:

Those seem to be the optimal settings

    echo -ne ">\x01\x05\x03\x07\x50\x50\x50" >| /dev/ttyACM3


Configurations
==============

for µC controlling Laminaflow- and Lasercutter-damper
-----------------------------------------------------

	PJON device id: 1
	PJON sensor destid: 0
	#Dampers: 3
	Damper0: is installed
	         pos: consid. open at: 80, current: 0, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor0: NOT installed
	Damper1: is installed
	         pos: consid. open at: 80, current: 0, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor1: NOT installed
	Damper2: NOT installed
	         pos: consid. open at: 80, current: 1, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor2: NOT installed


for µC controlling Fan and Corner-damper
-----------------------------------------------------

	PJON device id: 2
	PJON sensor destid: 0
	#Dampers: 3
	Damper0: NOT installed
	         pos: consid. open at: 80, current: 1, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor0: NOT installed
	Damper1: NOT installed
	         pos: consid. open at: 80, current: 1, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor1: NOT installed
	Damper2: is installed
	         pos: consid. open at: 80, current: 0, target: 0
	         endstop lightbeam: interrupted
	Pressure Sensor2: NOT installed


TODO
=====

- maybe better handle pjon_error_handler
