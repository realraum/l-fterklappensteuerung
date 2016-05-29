#include <stdio.h>
#include "Arduino.h"
//#include "BMP180.h"
#include "dampercontrol.h"

void pressure_sensors_init()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
  	//TODO: try to initialize each sensor via SPI
  	// set sensor_installed true on success
  	sensor_installed_[d] = false;
  }
}

void task_check_pressure()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
  	if (!sensor_installed_[d])
  		continue;
  	double pressure_mbar = 0.0;
  	//TODO: read preassure

  	//TODO: if read successfull.. (otherwise disable sensor)
  	pjon_send_pressure_infomsg(d, pressure_mbar);
  }
}