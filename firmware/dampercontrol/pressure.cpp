#include <stdio.h>
#include <avr/pgmspace.h>
#include <LUFA/Drivers/Peripheral/SPI.h>
#include "bmp280.h"
#include "dampercontrol.h"

bmp280_sensor psensor[NUM_DAMPER];
bmp280_result cur_temp_pressure[NUM_DAMPER];

uint8_t init_sensor_number(uint8_t d)
{
  switch (d)
  {
    default:
    case 0:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S0), PIN_CS_S0);
    case 1:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S1), PIN_CS_S1);
    case 2:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S2), PIN_CS_S2);
  }
}

void pressure_sensors_init()
{
  //SPI chipselect Pins
  PINMODE_OUTPUT(REG_CS_S0,PIN_CS_S0);
  PINMODE_OUTPUT(REG_CS_S1,PIN_CS_S1);
  PINMODE_OUTPUT(REG_CS_S2,PIN_CS_S2);
  CS_SENSOR(0,HIGHv);
  CS_SENSOR(1,HIGHv);
  CS_SENSOR(2,HIGHv);
  SPI_Init(BMP280_LUFA_SPIO_OPTIONS);

  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    //TODO: try to initialize each sensor via SPI
    // set sensor_installed true on success
    sensor_installed_[d] = init_sensor_number(d);
  }
}

void task_check_pressure()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    sensor_installed_[d] = ((sensor_installed_[d])? bmp280_check_chipid(&psensor[d]) : init_sensor_number(d));
    if (!sensor_installed_[d])
      continue;

    cur_temp_pressure[d] = bmp280_readTempAndPressure(&psensor[d]);
  }
}

float get_latest_pressure(uint8_t sensorid)
{
  return cur_temp_pressure[sensorid].pressure;
}

float get_latest_temperature(uint8_t sensorid)
{
  return cur_temp_pressure[sensorid].temperature;
}