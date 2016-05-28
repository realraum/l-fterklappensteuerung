#ifndef DAMPER_CONTROL_H
#define DAMPER_CONTROL_H


/* Hardware: AVR ATMEGA32U4
 * 
 * ==== PINS ====
 * PB5 .. Endstop 0  (PCINT5)
 * PB6 .. Endstop 1  (PCINT6)
 * PB7 .. Endstop 2  (PCINT7)
 * PD0... Damper Motor 0
 * PD1... Damper Motor 1
 * PD2... Damper Motor 2
 * PF0... Ventilation Fan
 * ???... PJON Pin
 * ???... SPI MOSI
 * ???... SPI MISO
 * ???... SPI CLK
 * ???... SPI Sensor0 CS
 * ???... SPI Sensor1 CS
 * ???... SPI Sensor2 CS
*/

#define PIN_PJON 1

#define PIN_HIGH(PORT, PIN) PORT |= (1 << PIN)
#define PIN_LOW(PORT, PIN) PORT &= ~(1 << PIN)
#define PINMODE_OUTPUT PIN_HIGH  //just use DDR instead of PORT
#define PINMODE_INPUT PIN_LOW  //just use DDR instead of PORT

#define OP_SETBIT |=
#define OP_CLEARBIT &= ~
#define OP_CHECK &
#define PIN_SW(PORTDDRREG, PIN, OP) PORTDDRREG OP (1 << PIN)

#define HIGHv OP_SETBIT
#define LOWv OP_CLEARBIT

//with F_CPU = 16MHz and TIMER3 Prescaler set to /1024, TIMER3 increments with f = 16KHz. Thus if TIMER3 reaches 16, 1ms has passed.
#define T3_MS     *16
//set TICK_TIME to 1/20 of a second
#define TICK_DURATION_IN_MS 50
#define TICK_TIME (TICK_DURATION_IN_MS T3_MS)

///// HARDWARE CONTROL DEFINES /////

#define ENDSTOP_0_ISHIGH PIN_SW(PORTB,PB5,OP_CHECK)
#define ENDSTOP_1_ISHIGH PIN_SW(PORTB,PB6,OP_CHECK)
#define ENDSTOP_2_ISHIGH PIN_SW(PORTB,PB7,OP_CHECK)
#define ENDSTOP_ISHIGH(x) PIN_SW(PORTB,(PB5+x),OP_CHECK)

#define DAMPER_MOTOR_RUN(x) PIN_SW(PORTB,(PD0+x),OP_SETBIT)
#define DAMPER_MOTOR_STOP(x) PIN_SW(PORTB,(PD0+x),OP_CLEARBIT)

#define FAN_RUN  PIN_SW(PORTF,PF0,OP_SETBIT)
#define FAN_STOP PIN_SW(PORTF,PF0,OP_CLEARBIT)
#define FAN_ISRUNNING PIN_SW(PORTF,PF0,OP_CHECK)


/// GLOBALS ///



#define NUM_DAMPER 3

enum pjon_msg_type_t {MSG_DAMPERCMD, MSG_PRESSUREINFO, MSG_ERROR, MSG_UPDATESETTINGS};
enum damper_cmds_t {DAMPER_CLOSED, DAMPER_OPEN, DAMPER_HALFOPEN};
enum fan_cmds_t {FAN_OFF, FAN_ON, FAN_AUTO};
enum error_type_t {NO_ERROR, DAMPER_CONTROL_TIMEOUT};
enum next_char_state_t {CCMD, CDEVID, CINSTALLEDDAMPERS, CPKTLEN, CPKT};


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

void pressure_sensors_init();
void task_check_pressure();


#endif