#ifndef DAMPER_CONTROL_H
#define DAMPER_CONTROL_H


/* Hardware: AVR ATMEGA32U4
 * 
 * ==== PINS ====
 * PB0... SPI Sensor0 CS
 * PB1... SPI CLK
 * PB2... SPI MOSI
 * PB3... SPI MISO
 * PB4 .. Endstop 0  (PCINT4)
 * PB5 .. Endstop 1  (PCINT5)
 * PB6 .. Endstop 2  (PCINT6)
 * PB7... SPI Sensor1 CS
 * PC6... SPI Sensor2 CS
 * PD0... Damper Motor 0
 * PD1... Damper Motor 1
 * PD2... Damper Motor 2
 * PD3... PJON Pin
 * PD6... Ventilation Fan (Teensy2LED)
 * PF0... =ADC0 needed by PJON
*/

#define PIN_CS_S0 PB0
#define REG_CS_S0 PINB
#define PIN_CS_S1 PB7
#define REG_CS_S1 PINB
#define PIN_CS_S2 PC6
#define REG_CS_S2 PINC
#define PIN_ENDSTOP_0 PB4
#define REG_ENDSTOP_0 PINB
#define PIN_ENDSTOP_1 PB5
#define REG_ENDSTOP_1 PINB
#define PIN_ENDSTOP_2 PB6
#define REG_ENDSTOP_2 PINB
#define PIN_DAMPER_0 PD0
#define REG_DAMPER_0 PIND
#define PIN_DAMPER_1 PD1
#define REG_DAMPER_1 PIND
#define PIN_DAMPER_2 PD2
#define REG_DAMPER_2 PIND
#define PIN_FAN PD6
#define REG_FAN PIND

//aka PD3 INT3
// see ../contrib/avr-utils/lib/arduino-leonardo/arduino_pins.h
#define PIN_PJON 1

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

#define FAN_RUN  PIN_SW(PORTD,PD6,OP_SETBIT)
#define FAN_STOP PIN_SW(PORTD,PD6,OP_CLEARBIT)
#define FAN_ISRUNNING (PIN_SW(PIND,PD6,OP_CHECK))

#define CS_SENSOR_0(LOWHIGH) (PIN_SW(PORTB,PB0,LOWHIGH))
#define CS_SENSOR_1(LOWHIGH) (PIN_SW(PORTB,PB7,LOWHIGH))
#define CS_SENSOR_2(LOWHIGH) (PIN_SW(PORTC,PC6,LOWHIGH))
#define CS_SENSOR(x,LOWHIGH) CS_SENSOR_#x(LOWHIGH)


/// GLOBALS ///



#define NUM_DAMPER 3

enum pjon_msg_type_t {MSG_DAMPERCMD, MSG_PRESSUREINFO, MSG_ERROR, MSG_UPDATESETTINGS};
enum damper_cmds_t {DAMPER_CLOSED, DAMPER_OPEN, DAMPER_HALFOPEN};
enum fan_cmds_t {FAN_OFF, FAN_ON, FAN_AUTO};
enum error_type_t {NO_ERROR, DAMPER_CONTROL_TIMEOUT};


typedef struct {
  uint8_t damper[NUM_DAMPER];
  uint8_t fan : 2;
} dampercmd_t;

typedef  struct {
  uint8_t sensorid;
  double mBar;
} pressureinfo_t;

typedef  struct {
  uint8_t damperid;
  uint8_t errortype;
} errorinfo_t;

typedef  struct {
  uint8_t damper_open_pos[NUM_DAMPER];
} updatesettings_t;


typedef struct {
  uint8_t type;
  union {
    dampercmd_t dampercmd;
    pressureinfo_t pressureinfo;
    errorinfo_t errorinfo;
    updatesettings_t updatesettings;
  };
} pjon_message_t;


extern bool damper_installed_[NUM_DAMPER];
extern bool sensor_installed_[NUM_DAMPER];
extern uint8_t damper_open_pos_[NUM_DAMPER];
extern uint8_t pjon_bus_id_;
extern uint8_t pjon_sensor_destination_id_;

bool are_all_dampers_closed(void);
bool have_dampers_reached_target(void);
inline void task_control_dampers(void);
void task_control_fan(void);
void task_check_pressure(void);
void task_pjon(void);
void task_usbserial(void);
void handle_damper_cmd(dampercmd_t *rxmsg);

void saveSettings2EEPROM();
void loadSettingsFromEEPROM();
void updateSettingsFromPacket(updatesettings_t *s);
void updateInstalledDampersFromChar(uint8_t damper_installed);
void pjon_init();
void pjon_change_busid(uint8_t id);
void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload);
void pjon_send_pressure_infomsg(uint8_t sensorid, double pressure);
void pjon_senderror_dampertimeout(uint8_t damperid);

void pressure_sensors_init();
void task_check_pressure();


#endif