#include <stdio.h>
#include "usbio.h"
#include "Arduino.h"
#include "PJON.h"
#include "dampercontrol.h"

PJON<SoftwareBitBang> pjonbus_;

void pjon_error_handler(uint8_t code, uint8_t data)
{
  if(code == CONNECTION_LOST) {
    printf("Connection with device ID %d is lost.\r\n", data);
  }
  if(code == PACKETS_BUFFER_FULL) {
    printf("Packet buffer is full, has now a length of %d\r\n", data);
    printf("Possible wrong bus configuration!\r\n");
    printf("higher MAX_PACKETS in PJON.h if necessary.\r\n");
  }
  if(code == MEMORY_FULL) {
    printf("Packet memory allocation failed. Memory is full.\r\n");
  }
  if(code == CONTENT_TOO_LONG) {
    printf("Content is too long, length: %d\r\n", data);
  }
  if(code == ID_ACQUISITION_FAIL) {
    printf("Can't acquire a free id %d\r\n", data);
  }
}

bool pjon_assert_length(uint8_t length, uint8_t expected_length)
{
  if (length != expected_length)
  {
    printf("PJON Msg got: %d, expected: %d\n", length, expected_length);
    return false;
  } else {
    return true;
  }
}

void pjon_recv_handler(uint8_t id, uint8_t *payload, uint8_t length)
{
  printf("got message(%d bytes) from %d: '", length, id);
  if(length == 0) {
    printf("\r\n");
    return;
  }  
  for(uint16_t i = 0; i < length; ++i)
    printf("%c",payload[i]);
  printf("'\r\n");
  
  if (!pjon_assert_length(length, sizeof(pjon_message_t)))
  {
    return;
  }

  pjon_message_t *msg = (pjon_message_t *) payload;

  switch(msg->type)
  {
    case MSG_DAMPERCMD:
      handle_damper_cmd(&(msg->dampercmd));
      break;
    case MSG_PRESSUREINFO:
      break;
    case MSG_ERROR:
      break;
    case MSG_UPDATESETTINGS:
      break;
    default:
      printf("Unknown MSG type %d\n", payload[0]);
      break;
  }
}

void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload)
{
  pjon_recv_handler(pjon_bus_id_, payload, length);
  //hope we did not mangle the payload
  pjonbus_.send(BROADCAST, (const char*) payload, length);
}

void pjon_send_pressure_infomsg(uint8_t sensorid, double pressure)
{
  pjon_message_t msg;
  msg.type = MSG_PRESSUREINFO;
  msg.pressureinfo.sensorid = sensorid;
  msg.pressureinfo.mBar = pressure;
  pjonbus_.send(pjon_sensor_destination_id_, (char*) &msg, sizeof(pressureinfo_t)+1);
}

void pjon_change_busid(uint8_t id)
{
  pjon_bus_id_ = id;
  pjonbus_.set_id(pjon_bus_id_);
  saveSettings2EEPROM();
}

void pjon_init()
{
  arduino_init();
  pjonbus_.set_error(pjon_error_handler);
  pjonbus_.set_receiver(pjon_recv_handler);
  pjonbus_.set_pin(PIN_PJON);
  pjonbus_.set_id(pjon_bus_id_);
  pjonbus_.begin();
}

void task_pjon()
{
    pjonbus_.update();
    pjonbus_.receive(20); 
}