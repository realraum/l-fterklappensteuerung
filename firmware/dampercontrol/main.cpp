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
#include <util/delay.h>
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
//   we start at 1 in order to seek the 0 position at startup via the endstop
uint8_t damper_states_[NUM_DAMPER] = {1,1,1};

//damper target states: the state that damper states is supposed to reach
uint8_t damper_target_states_[NUM_DAMPER] = {0,0,0};

bool damper_state_overflowed_[NUM_DAMPER] = {false,false,false};

bool fan_state_ = false;
uint8_t fan_target_state_ = FAN_AUTO;

// ISR sets true if photoelectric fork x went low
bool damper_endstop_reached_[NUM_DAMPER];

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
  PCICR = _BV(PCIE0);
  //set up Endstop PinChange Interrupts toggle interrupt
  PCMSK0 = (1<<PCINT4) | (1<<PCINT5) | (1<<PCINT6);
}

void initPINs()
{
  //PJON pin is intialized by pjon_init
  //endstop inputs
  PINMODE_INPUT(REG_ENDSTOP_0,PIN_ENDSTOP_0);
  PINMODE_INPUT(REG_ENDSTOP_1,PIN_ENDSTOP_1);
  PINMODE_INPUT(REG_ENDSTOP_2,PIN_ENDSTOP_2);
  //enable internal pullups
  PIN_HIGH(REG_ENDSTOP_0,PIN_ENDSTOP_0);
  PIN_HIGH(REG_ENDSTOP_1,PIN_ENDSTOP_1);
  PIN_HIGH(REG_ENDSTOP_2,PIN_ENDSTOP_2);
  //set damper and fan motor as output
  PINMODE_OUTPUT(REG_DAMPER_0,PIN_DAMPER_0);
  PINMODE_OUTPUT(REG_DAMPER_2,PIN_DAMPER_1);
  PINMODE_OUTPUT(REG_DAMPER_2,PIN_DAMPER_2);
  //PD3 is pjon, maybe it can use the INT4 someday
  PINMODE_OUTPUT(REG_FAN,PIN_FAN); //FAN
  DAMPER_MOTOR_STOP(0);
  DAMPER_MOTOR_STOP(1);
  DAMPER_MOTOR_STOP(2);
  FAN_STOP;
}

/*
//Bad Idea
void initGuessPositionFromEndstop()
{
  _delay_ms(50); // give Endstop Pins time to settle
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_endstop_reached_[d] = false;
    if (ENDSTOP_ISHIGH(d))
    {
      //if not at endstop on boot, assume we are open
      damper_states_[d] = damper_open_pos_[d];
      damper_target_states_[d] = damper_open_pos_[d];
    } else {
      damper_states_[d] = 0;
      damper_target_states_[d] = 0;      
    }
  }    
}*/

//note: not installed dampers are always closed
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

//return yes if the endstop of damperX was toggled since last time we asked
bool did_damper_pass_endstop(uint8_t damperid)
{
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

//called by pinchange interrupt
void task_check_endstops()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (ENDSTOP_ISHIGH(d) == 0)
    {
      damper_endstop_reached_[d] = true;
    }
  }  
}

void task_simulate_pinchange_interrupt()
{
  bool interrupt=false;
  static uint8_t last_state[3] = {0,0,0};
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    uint8_t curstate = ENDSTOP_ISHIGH(d);
    if (curstate != last_state[d])
    {
      printf("pinchange on endstop %d, state:%d\r\n", d, curstate);
      interrupt=true;
      last_state[d]=curstate;
    }
  }
  if (interrupt)
    task_check_endstops();
}

//called by timer
void task_control_dampers()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (!damper_installed_[d])
      continue;

    //here we self-synchronize the position time counter
    if (did_damper_pass_endstop(d))
      damper_states_[d] = 0;

    //send warning, since we timed out and that might mean the endstop does not work
    //(0x80 << sizeof(damper_states_[0]))-1 is the max-value of uint8_t aka 0xFF
    if (damper_target_states_[d] == 0 && damper_states_[d] == (0x80 << sizeof(damper_states_[0]))-1)
      damper_state_overflowed_[d] = true;

    if (damper_states_[d] != damper_target_states_[d])
    {
      //move motor
      DAMPER_MOTOR_RUN(d);
      //increment position time counter if we are moving
      damper_states_[d]++;
      // printf("Motor %d Run @%d\r\n", d, damper_states_[d]);
    } else {
      //stop motor
      DAMPER_MOTOR_STOP(d);
      // printf("Motor %d Stop @%d\r\n", d, damper_states_[d]);
    }
  }
}

ISR(TIMER3_COMPA_vect)
{
  //called every TIME_TICK (aka 8ms)
  task_control_dampers();
  //set up "clock" comparator for next tick
  OCR3A = (OCR3A + TICK_TIME) & 0xFFFF;
/*  if (debug_)
    led_toggle();
*/
}

//https://sites.google.com/site/qeewiki/books/avr-guide/external-interrupts-on-the-atmega328
ISR(PCINT0_vect)
{
  task_check_endstops();
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

void task_check_damper_state_overflow()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (damper_state_overflowed_[d])
    {
      damper_state_overflowed_[d] = false;
      pjon_senderror_dampertimeout(d);
    }
  }
}

enum next_char_state_t {CCMD, CDEVID, CINSTALLEDDAMPERS, CPKTLEN, CPKT};

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

void printSettings()
{
  printf("=== State ===\r\n");
  printf("PJON device id: %d\r\n", pjon_bus_id_);
  printf("PJON sensor destid: %d\r\n", pjon_sensor_destination_id_);
  printf("#Dampers: %d\r\n", NUM_DAMPER);
  for (uint8_t d=0; d<NUM_DAMPER; d++) {
    printf("Damper%d: %s installed\r\n", d, (damper_installed_[d])?"is":"NOT");
    printf("\t pos: consid. open at: %d, current: %d, target: %d\r\n", damper_open_pos_[d],damper_states_[d],damper_target_states_[d]);
    printf("\t endstop lightbeam: %s\r\n", (ENDSTOP_ISHIGH(d))?"interrupted":"uninterrupted");
    printf("Pressure Sensor%d: %s installed\r\n", d, (sensor_installed_[d])?"is":"NOT");
    if (sensor_installed_[d])
    {
      printf("\t Pressure: %.2f Pa @ %.2f degC\r\n", get_latest_pressure(d), get_latest_temperature(d));
    }
  }
  printf("Fan is %s and set to %d\r\n", (fan_state_)?"on":"off", fan_target_state_);
}

void handle_serialdata(char c)
{
  static next_char_state_t next_char = CCMD;
  static uint8_t read_num_chars = 0;
  static uint8_t msg_len = 0;
  static uint8_t msg_buf[0xff];

  switch (next_char) {
    default:
    case CCMD:
      switch(c) {
        case 'P': next_char = CDEVID; break;
        case 'I': next_char = CINSTALLEDDAMPERS; break;
        case 'o': damper_target_states_[0] = damper_open_pos_[0]; printf("opening Damper0\r\n"); break;
        case 'c': damper_target_states_[0] = 0; printf("closing Damper0\r\n"); break;
        case 'h': damper_target_states_[0] = damper_open_pos_[0]/2; printf("half-open Damper0\r\n"); break;
        case '>': next_char = CPKTLEN; break;
        case 's': printSettings(); break;
        case '!': reset2bootloader(); break;
      }
    break;
    case CDEVID:
      pjon_change_busid(c - '0');
      printf("device id is now: %d\r\n", c - '0');
      next_char = CCMD;
    break;
    case CINSTALLEDDAMPERS:
      updateInstalledDampersFromChar(c - '0');
      printf("installed dampers updated\r\n");
      next_char = CCMD;
    break;
    case CPKTLEN:
      if (c == 0) {
        next_char = CCMD;
        break;
      }
      read_num_chars = c;
      msg_len = 0;
      next_char = CPKT;
      break;
    case CPKT:
      msg_buf[msg_len++] = c;
      read_num_chars--;
      if (read_num_chars == 0)
      {
        pjon_inject_broadcast_msg(msg_len, msg_buf);
        next_char = CCMD;
      }
      break;
  }
}

int main()
{
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  cpu_init();
  led_init();
  usbio_init();

  // init
  loadSettingsFromEEPROM();
  pjon_init(); //PJON first since it calls arduino init which might do who knows what
  initPINs();
  // initGuessPositionFromEndstop(); //does not work well, since we never really stop exactly at the endstop
  initSysClkTimer3();
  initPCInterrupt();
  sei();
  pressure_sensors_init();

  uint16_t loop_count = 0;

  // loop
  for (;;)
  {
    int16_t BytesReceived = usbio_bytes_received();
    while(BytesReceived > 0)
    {
      int ReceivedByte = fgetc(stdin);
      if(ReceivedByte != EOF)
      {
        handle_serialdata(ReceivedByte);
      }
      BytesReceived--;
    }

    usbio_task();
    if ((loop_count & 0xFFF) == 0)
      task_check_pressure();
    if ((loop_count & 0xFFFF) == 0)
    {
      for (uint8_t d=0; d<NUM_DAMPER; d++)
      {
        if (sensor_installed_[d])
        {
          pjon_send_pressure_infomsg(d, get_latest_pressure(d), get_latest_temperature(d));
        }
      }
    }
    task_pjon();
    //task_control_dampers(); // called by timer in precise intervals, do not call from loop
    //task_simulate_pinchange_interrupt();
    task_control_fan();
    task_check_damper_state_overflow();
    loop_count++;
  }
}

