# r3 Lüfterklappensteuerung

Damper-Control for ventilation exhaust system in realraum.

## Overview

The state of three dampers (open / closed) and one fan (running / not running)need to be controlled electronically.
Additionally two touchscreen interfaces shall be created to controll the damper states.

The three dampers control exhaust of :
- Lasercutter
- Atomic absorption spectrometer
- Laminaflow

The lasercutter is in a separate room with a separate touchscreen interface.

##### Constraints:

If a damper has been opened in one room, it can only be closed via the interface in that room. This is a safty measure so that dampers 
are not closed from a separate room by people unaware that the exhaust is still in use.


## Contents

### elektronik

see this folder on how to connect stuff.

### firmware

this folder contains the firmware for the uc

The µCs communicate via PJON protocol. At least one raspberry will communicate via usb-serial with one of the µC. Each µC can be a bridge between usb-serial and PJON-bus.

### mechanik

The dampers are controlled by cheap motors from neuhold.
Their position is known via a photoelectric fork and a lasercut disk with slots.
