#include <avr.h>
#include <avr/eeprom.h>
#include "dampercontrol.h"

#define EEPROM_DATA_VERSION 0

void saveSettings2EEPROM()
{
	uint8_t eeprom_pos=0;
	uint8_t damper_installed=0;

	eeprom_write_byte((void*) eeprom_pos++, EEPROM_DATA_VERSION);
	eeprom_write_byte((void*) eeprom_pos++, pjon_bus_id_);
	eeprom_write_byte((void*) eeprom_pos++, NUM_DAMPER);
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
		eeprom_write_byte((void*) eeprom_pos++, damper_open_pos_[d]);
		if (damper_installed_[d])
			damper_installed |= _BV(d)
	}
	eeprom_write_byte((void*) eeprom_pos++, damper_installed);
}

void loadSettingsFromEEPROM()
{
	uint8_t eeprom_pos=0;

	if (eeprom_read_byte((void*) eeprom_pos++) != EEPROM_DATA_VERSION)
		return;
	pjon_bus_id_ = eeprom_read_byte((void*) eeprom_pos++);
	if (eeprom_read_byte((void*) eeprom_pos++) != NUM_DAMPER)
		return;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
		damper_open_pos_[d] = eeprom_read_byte((void*) eeprom_pos++);
	}
	uint8_t damper_installed = eeprom_read_byte((void*) eeprom_pos++);
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
  	damper_installed_[d] = 0 < _BV(d) & damper_installed;
  }
}

void updateSettingsFromPacket(update_settings_t *s)
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
  	damper_installed_[d] = 0 < _BV(d) & s->settings->damper_installed;
  	damper_open_pos_[d] =  s->settings->damper_open_pos;
  }
  saveSettings2EEPROM();
}

