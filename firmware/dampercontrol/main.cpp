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

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/power.h>
#include <stdio.h>

#include "util.h"
#include "led.h"
#include "usbio.h"

#include "dampercontrol.h"


//damper states: 0 means closed (means photoelectric fork sensor pulled LOW)
//         otherwise, time units since we enabled the motor
//               100 should be open
//               if we go over 110 without the photoelectric fork sensor signaling us, we raise an error
uint8_t damper_states_[NUM_DAMPER] = {0,0,0};

//damper target states: the state that damper states is supposed to reach
uint8_t damper_target_states_[NUM_DAMPER] = {0,0,0};

bool fan_state_ = false;
uint8_t fan_target_state_ = FAN_AUTO;


////// HELPER FUNCTIONS //////

void initSysClkTimer3(void)
{
  // set counter to 0
  TCNT3 = 0x0000;
  // no outputs
  TCCR3A = 0;
  // Prescaler for Timer3: F_CPU / 1024 -> counts with f= 16KHz ms
  TCCR3B = _BV(CS32) | _BV(CS30);
  // set up "clock" comparator for first tick
  OCR3A = TICK_TIME & 0xFFFF;
  // enable interrupt
  TIMSK3 = _BV(OCIE3A);
}

void initPCInterrupt(void)
{
  //enable PinChange Interrupt
  PCICR = (1 << PCIE0);
  //set up PB5,6,7 to toggle interrupt
  PCMSK0 = (1<<PCINT7) || (1<<PCINT6) || (1<<PCINT5);
}

bool are_all_dampers_closed()
{
  bool rv = true;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    rv &= (damper_states_[d] == 0) || !damper_installed_[d];
  }
  return rv;
}

bool have_dampers_reached_target()
{
  bool rv = true;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    rv &= (damper_states_[d] == damper_target_states_[d]) || !damper_installed_[d];
  }
  return rv;
}

// TODO Set a bit if photoelectric fork x went low
bool damper_endstop_reached_[NUM_DAMPER];

//return yes if the endstop of damperX was toggled since last time we asked
bool did_damper_pass_endstop(uint8_t damperid)
{
  //TODO
  bool rv = damper_endstop_reached_[damperid];
  //reset endstop bit, but only if we saw it set (to guard against race condition)
  //the assumption being, that it could just now have been set to true
  //but we check so much more often while the motor is turning that it's unlikely
  //we'll set a bit to false that was just set to true again
  if (rv)
    damper_endstop_reached_[damperid] = false;
  return rv;
}

/// INTERRUPT ROUTINES

/* ISR writes:
 - damper_states_
 - switch_damper_motor
 
 * ISR reads:
 - damper_target_states_

 * ISR only use (read/write):
 - damper_endstop_reached

*/

//called by timer
inline void task_control_dampers()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (!damper_installed_[d])
      continue;

    if (did_damper_pass_endstop(d))
      damper_states_[d] = 0;

    if (damper_states_[d] != damper_target_states_[d])
    {
      DAMPER_MOTOR_RUN(d);
      damper_states_[d]++;
    } else {
      DAMPER_MOTOR_STOP(d);
    }
  }
}

ISR(TIMER3_COMPA_vect)
{
  //called every TIME_TICK (aka 50ms)
  task_control_dampers();
  //set up "clock" comparator for next tick
  OCR3A = (OCR3A + TICK_TIME) & 0xFFFF;
/*  if (debug_)
    led_toggle();
*/
}

ISR(PCINT0_vect)
{
  if (! ENDSTOP_0_ISHIGH)
    damper_endstop_reached_[0] = true;
  if (! ENDSTOP_1_ISHIGH)
    damper_endstop_reached_[1] = true;
  if (! ENDSTOP_2_ISHIGH)
    damper_endstop_reached_[2] = true;
}

/// TASKS

void task_control_fan()
{
  switch (fan_target_state_) {
    case FAN_OFF:
    // switching off can always be done right away
    FAN_STOP;
    break;
    
    default:
    case FAN_AUTO:
    case FAN_ON:
    // once dampers have reached their target and if at least one damper is not closed, we switch the fan on
    if (have_dampers_reached_target() && !are_all_dampers_closed())
      FAN_RUN;
    else
      FAN_STOP;
    break;
  }
}

void task_usbserial(void)
{

}


void handle_damper_cmd(dampercmd_t *rxmsg)
{
  switch (rxmsg->fan)
  {
    case FAN_AUTO:
    case FAN_ON:
    case FAN_OFF:
      fan_target_state_ = rxmsg->fan;
      break;
    default:
      fan_target_state_ = FAN_OFF;
      break;
  }
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    switch(rxmsg->damper[d])
    {
      default:
      case DAMPER_CLOSED:
        damper_target_states_[d] = 0;
        break;
      case DAMPER_OPEN:
        damper_target_states_[d] = damper_open_pos_[d];
        break;
      case DAMPER_HALFOPEN:
        damper_target_states_[d] = damper_open_pos_[d] / 2;
        break;
    }
  }
}

void task_check_pressure()
{

}

void handle_serialdata(char c)
{
  switch(c) {
  case '1': case '2': case '3':
  case '4': case '5': case '6':
  case '7': case '8': case '9': pjon_change_busid(c - '0'); printf("device id is now: %d\r\n", c - '0'); break;
  case '!': reset2bootloader(); break;
  }
}

int main()
{
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  cpu_init();
  led_init();
  usbio_init();
  sei();

  // init
  loadSettingsFromEEPROM();
  pjon_init();


  // loop
  for (;;)
  {
    int16_t BytesReceived = usbio_bytes_received();
    while(BytesReceived > 0) {
      int ReceivedByte = fgetc(stdin);
      if(ReceivedByte != EOF) {
        handle_serialdata(ReceivedByte);
      }
      BytesReceived--;
    }

    usbio_task();
    task_check_pressure();
    task_pjon();
    task_usbserial();
    // task_control_dampers();
    task_control_fan();
  }
}

