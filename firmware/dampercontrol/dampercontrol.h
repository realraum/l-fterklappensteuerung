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

#ifndef DAMPER_CONTROL_H
#define DAMPER_CONTROL_H


/* Hardware: AVR ATMEGA32U4 (Arduino Pro Micro)
 * PinOut: https://deskthority.net/wiki/Arduino_Pro_Micro
 *
 *
 * ==== PINS ====
 * PD0... SPI Sensor1 CS
 * PD1... SPI Sensor0 CS
 * PD4... SPI Sensor2 CS
 * PD2... RX
 * PD3....TX
 * PB1... SPI CLK
 * PB2... SPI MOSI
 * PB3... SPI MISO
 * PB4 .. Endstop 0  (PCINT4)
 * PB5 .. Endstop 1  (PCINT5)
 * PB6 .. Endstop 2  (PCINT6)
 * PF4... Damper Motor 0
 * PF5... Damper Motor 1
 * PF6... Damper Motor 2
 * PC6... Ventilation Fan of Laminaflow
 * PD7... PJON Pin
 * PE6... Main Ventilation Fan (Teensy2LED)
 * PF0/PF7... =ADC0 needed by PJON
*/

#define PIN_CS_S0 PD1
#define REG_CS_S0 PIND
#define PIN_CS_S1 PD0
#define REG_CS_S1 PIND
#define PIN_CS_S2 PD4
#define REG_CS_S2 PIND
#define PIN_ENDSTOP_0 PB4
#define REG_ENDSTOP_0 PINB
#define PIN_ENDSTOP_1 PB5
#define REG_ENDSTOP_1 PINB
#define PIN_ENDSTOP_2 PB6
#define REG_ENDSTOP_2 PINB
#define PIN_DAMPER_0 PF4
#define REG_DAMPER_0 PINF
#define PIN_DAMPER_1 PD5
#define REG_DAMPER_1 PINF
#define PIN_DAMPER_2 PD6
#define REG_DAMPER_2 PINF
#define PIN_FAN PE6
#define REG_FAN PINE
#define PIN_FANLAMINA PC6
#define REG_FANLAMINA PINC

//aka PD7
// see ../contrib/avr-utils/lib/arduino-leonardo/pins_arduino.h
#define PIN_PJON 6

#define PINREG(x) x
#define DDRREG(x) *(&x+1)
#define PORTREG(x) *(&x+2)

#define PIN_HIGH(REG, PIN) *(&REG+2) |= (1 << PIN)
#define PIN_LOW(REG, PIN) *(&REG+2) &= ~(1 << PIN)
#define PINMODE_OUTPUT(REG, PIN) *(&REG+1) |= (1 << PIN)  //just use DDR instead of PORT
#define PINMODE_INPUT(REG, PIN) *(&REG+1) &= ~(1 << PIN)  //just use DDR instead of PORT
#define IS_HIGH(REG,PIN) REG & (1 << PIN)

#define OP_SETBIT |=
#define OP_CLEARBIT &= ~
#define OP_CHECK &
#define PIN_SW(PORTDDRREG, PIN, OP) PORTDDRREG OP (1 << PIN)


#define HIGHv OP_SETBIT
#define LOWv OP_CLEARBIT

//with F_CPU = 16MHz and TIMER3 Prescaler set to /1024, TIMER3 increments with f = 16KHz. Thus if TIMER3 reaches 16, 1ms has passed.
#define T3_MS     *16
//set TICK_TIME to 1/20 of a second
#define TICK_DURATION_IN_MS 8
#define TICK_TIME (TICK_DURATION_IN_MS T3_MS)

///// HARDWARE CONTROL DEFINES /////

#define ENDSTOP_0_ISHIGH (PINREG(REG_ENDSTOP_0) & _BV(PIN_ENDSTOP_0))
#define ENDSTOP_1_ISHIGH (PINREG(REG_ENDSTOP_1) & _BV(PIN_ENDSTOP_1))
#define ENDSTOP_2_ISHIGH (PINREG(REG_ENDSTOP_2) & _BV(PIN_ENDSTOP_2))
#define ENDSTOP_ISHIGH(x) (PINREG(REG_ENDSTOP_0) & _BV(PIN_ENDSTOP_0 + x))

#define DAMPER_MOTOR_RUN(x) PORTREG(REG_DAMPER_0) |= _BV(PIN_DAMPER_0 + x)
#define DAMPER_MOTOR_STOP(x) PORTREG(REG_DAMPER_0) &= ~_BV(PIN_DAMPER_0 + x)

#define FAN_RUN  PIN_LOW(REG_FAN,PIN_FAN)
#define FAN_STOP PIN_HIGH(REG_FAN,PIN_FAN)
#define FAN_ISRUNNING ((PORTREG(REG_FAN) & _BV(PIN_FAN)) == 0)

#define FANLAMINA_RUN  PIN_LOW(REG_FANLAMINA,PIN_FANLAMINA)
#define FANLAMINA_STOP PIN_HIGH(REG_FANLAMINA,PIN_FANLAMINA)
#define FANLAMINA_ISRUNNING ((PORTREG(REG_FANLAMINA) & _BV(PIN_FANLAMINA)) == 0)

#define CS_SENSOR_0(LOWHIGH) (PIN_SW(PORTREG(REG_CS_S0),PIN_CS_S0,LOWHIGH))
#define CS_SENSOR_1(LOWHIGH) (PIN_SW(PORTREG(REG_CS_S1),PIN_CS_S1,LOWHIGH))
#define CS_SENSOR_2(LOWHIGH) (PIN_SW(PORTREG(REG_CS_S2),PIN_CS_S2,LOWHIGH))
#define CS_SENSOR(x,LOWHIGH) CS_SENSOR_##x(LOWHIGH)


/// GLOBALS ///

#define NUM_DAMPER 3

enum pjon_msg_type_t {MSG_DAMPERCMD, MSG_PRESSUREINFO, MSG_ERROR, MSG_UPDATESETTINGS, MSG_PJONID_DOAUTO, MSG_PJONID_QUESTION, MSG_PJONID_INFO, MSG_PJONID_SET};
enum damper_cmds_t {DAMPER_CLOSED, DAMPER_OPEN, DAMPER_HALFOPEN};
enum fan_cmds_t {FAN_OFF, FAN_ON, FAN_AUTO};
enum error_type_t {NO_ERROR, DAMPER_CONTROL_TIMEOUT};


typedef struct {
  uint8_t damper[NUM_DAMPER];
  uint8_t fan : 2;
  uint8_t fanlamina : 2;
} dampercmd_t;

typedef struct {
  uint8_t sensorid;
  float celsius;
  float pascal;
} pressureinfo_t;

typedef struct {
  uint8_t damperid;
  uint8_t errortype;
} errorinfo_t;

typedef struct {
  uint8_t damper_open_pos[NUM_DAMPER];
} updatesettings_t;

typedef struct {
  uint8_t pjon_id;
} pjonidsetting_t;

typedef struct {
  uint8_t reach; // bitfield to indicate which hardware saw this packet: damper0, damper1, damper2, fan
  union {
    dampercmd_t dampercmd;
    updatesettings_t updatesettings;
  };
} pjon_chaincast_t;

typedef struct {
  uint8_t type;
  union {
    pjon_chaincast_t chaincast;
    pressureinfo_t pressureinfo;
    errorinfo_t errorinfo;
    pjonidsetting_t pjonidsetting;
  };
} pjon_message_t;

typedef struct {
  uint8_t id;
  uint8_t length;
  pjon_message_t msg;
} pjon_message_with_sender_t;

extern bool damper_installed_[NUM_DAMPER];
extern bool sensor_installed_[NUM_DAMPER];
extern uint8_t damper_open_pos_[NUM_DAMPER];
extern uint8_t pjon_device_id_;
extern uint8_t pjon_sensor_destination_id_;

bool are_all_dampers_closed(void);
bool have_dampers_reached_target(void);
inline void task_control_dampers(void);
void task_control_fan(void);
void task_check_pressure(void);
void task_pjon(void);
void task_usbserial(void);
void handle_damper_cmd(bool didreachall, dampercmd_t *rxmsg);

void saveSettings2EEPROM();
void loadSettingsFromEEPROM();
void updateSettingsFromPacket(updatesettings_t *s);
void updateInstalledDampersFromChar(uint8_t damper_installed);
uint8_t getInstalledDampersAsBitfield();

void pjon_init();
void pjon_change_deviceid(uint8_t id);
void pjon_inject_msg(uint8_t dst, uint8_t length, uint8_t *payload);
void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload);
void pjon_send_pressure_infomsg(uint8_t sensorid, float temperature, float pressure);
void pjon_senderror_dampertimeout(uint8_t damperid);
void pjon_send_dampercmd(dampercmd_t dcmd);
void pjon_chaincast_forward(uint8_t fromid, bool didreachall, pjon_message_t* msg);
void pjon_identify_myself(uint8_t toid);
void pjon_startautoiddiscover();
void pjon_become_master_of_ids();
void pjon_broadcast_get_autoid();

void pressure_sensors_init();
void task_check_pressure();
float get_latest_pressure(uint8_t sensorid);
float get_latest_temperature(uint8_t sensorid);

#endif