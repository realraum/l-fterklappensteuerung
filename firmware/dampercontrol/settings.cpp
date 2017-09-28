/*
 *  Damper Control Firmware
 *
 *
 *  Copyright (C) 2016 Bernhard Tittelbach <xro@realraum.at>
 *
 *  This software is made with love and spreadspace avr utils.
 *
 *  Damper Control Firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with these files. If not, see <http://www.gnu.org/licenses/>.
*/

#include <avr/eeprom.h>
#include <stdio.h>
#include "dampercontrol.h"

#define EEPROM_DATA_VERSION 1


//read this from eeprom on start
//update on receiving installed_msg
//tells us which damper is actually controlled by this µC
bool damper_installed_[NUM_DAMPER] = {false, false, false};
bool sensor_installed_[NUM_DAMPER] = {false, false, false};

//damper time divisor:
//every millis that we increase a damper_state if damper is currently moving
//128 is a good value for TICK_DURATION_IN_MS == 7
//103 is a good value for TICK_DURATION_IN_MS == 8
//90 is a good value for TICK_DURATION_IN_MS == 10
// in Theory it would be great to time the half-rotations of the damper perfectly
// so that we would not have to rely on the endstop.
// unfortunately time and damper position correlate very poorly
// so this does not work out. Thus we have to choose a TICK_DURATION_IN_MS > 7 and a damper_open_pos < 128.
// This way we can at least garantee that we always stop at the endstop (if the endstop works) if we close.
// Otherwise the damper_state_ position might overflow and reach 0 before we are at the endstop.
uint8_t damper_open_pos_[NUM_DAMPER] = {80,80,80};

uint8_t pjon_device_id_ = 255; //not assigned
uint8_t pjon_sensor_destination_id_ = 0; //BROADCAST


void saveSettings2EEPROM()
{
  uint8_t *eeprom_pos=0;
  uint8_t damper_installed=0;

  eeprom_write_byte(eeprom_pos++, EEPROM_DATA_VERSION);
  eeprom_write_byte(eeprom_pos++, pjon_device_id_);
  eeprom_write_byte(eeprom_pos++, NUM_DAMPER);
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    eeprom_write_byte(eeprom_pos++, damper_open_pos_[d]);
    if (damper_installed_[d])
      damper_installed |= _BV(d);
  }
  eeprom_write_byte(eeprom_pos++, damper_installed);
}

void loadSettingsFromEEPROM()
{
  uint8_t *eeprom_pos=0;

  if (eeprom_read_byte(eeprom_pos++) != EEPROM_DATA_VERSION)
    return;
  pjon_device_id_ = eeprom_read_byte(eeprom_pos++);
  if (eeprom_read_byte(eeprom_pos++) != NUM_DAMPER)
    return;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_open_pos_[d] = eeprom_read_byte(eeprom_pos++);
  }
  uint8_t damper_installed = eeprom_read_byte(eeprom_pos++);
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_installed_[d] = 0 < (_BV(d) & damper_installed);
  }
}

void updateSettingsFromPacket(updatesettings_t *s)
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_open_pos_[d] =  s->damper_open_pos[d];
  }
  saveSettings2EEPROM();
}

void updateInstalledDampersFromChar(uint8_t damper_installed)
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_installed_[d] = 0 < (_BV(d) & damper_installed);
  }
  saveSettings2EEPROM();
}

uint8_t getInstalledDampersAsBitfield()
{
  uint8_t rv = 0;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (damper_installed_[d])
      rv |= _BV(d);
  }
  return rv;
}