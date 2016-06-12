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

uint8_t pjon_type_to_msg_length(uint8_t type)
{
  switch(type)
  {
    case MSG_DAMPERCMD:
      return sizeof(dampercmd_t)+1;
      break;
    case MSG_PRESSUREINFO:
      return sizeof(pressureinfo_t)+1;
      break;
    case MSG_ERROR:
      return sizeof(errorinfo_t)+1;
      break;
    case MSG_UPDATESETTINGS:
      return sizeof(updatesettings_t)+1;
      break;
    default:
      return 0;
      break;
  }
}

void pjon_printf_msg(uint8_t id, uint8_t *payload, uint8_t length)
{
  printf("<%02x%02x", id, length);
  for(uint16_t i = 0; i < length; ++i)
    printf("%02x",payload[i]);
  printf("\r\n");
}

void pjon_recv_handler(uint8_t id, uint8_t *payload, uint8_t length)
{
  printf("got message(%d bytes) from %d: '", length, id);
  if(length < 1) {
    //accepting no messages without a type
    printf("\r\n");
    return;
  }  

  pjon_message_t *msg = (pjon_message_t *) payload;

  if (!pjon_assert_length(length, pjon_type_to_msg_length(msg->type)))
  { return; }

  pjon_printf_msg(id, payload, length);

  switch(msg->type)
  {
    case MSG_DAMPERCMD:
      printf("got msg_dampercmd\r\n");
      handle_damper_cmd(&(msg->dampercmd));
      break;
    case MSG_PRESSUREINFO:
      printf("got MSG_PRESSUREINFO\r\n");
      break;
    case MSG_ERROR:
      printf("got MSG_ERROR\r\n");
      break;
    case MSG_UPDATESETTINGS:
      printf("got MSG_UPDATESETTINGS\r\n");
      updateSettingsFromPacket(&(msg->updatesettings));
      break;
    default:
      printf("Unknown MSG type %d\n", msg->type);
      break;
  }
}

void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload)
{
  pjon_recv_handler(pjon_bus_id_, payload, length);
  //hope we did not mangle the payload
  pjonbus_.send(BROADCAST, (const char*) payload, length);
}

void pjon_send_pressure_infomsg(uint8_t sensorid, float pressure, float temperature)
{
  pjon_message_t msg;
  msg.type = MSG_PRESSUREINFO;
  msg.pressureinfo.sensorid = sensorid;
  msg.pressureinfo.celsius = temperature;
  msg.pressureinfo.pascal = pressure;
  pjon_printf_msg(pjon_sensor_destination_id_, (uint8_t*) &msg, pjon_type_to_msg_length(msg.type));
  pjonbus_.send(pjon_sensor_destination_id_, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

//sent if damper_states overflows before reaching endstop. May indicate defect endstop!!
void pjon_senderror_dampertimeout(uint8_t damperid)
{
  pjon_message_t msg;
  msg.type = MSG_ERROR;
  msg.errorinfo.damperid = damperid;
  msg.errorinfo.errortype = DAMPER_CONTROL_TIMEOUT;
  pjon_printf_msg(pjon_sensor_destination_id_, (uint8_t*) &msg, pjon_type_to_msg_length(msg.type));
  pjonbus_.send(pjon_sensor_destination_id_, (char*) &msg, pjon_type_to_msg_length(msg.type));
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