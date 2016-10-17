#include <stdio.h>
#include <util/delay.h>
#include "usbio.h"
#include "Arduino.h"
#include "PJON.h"
#include "dampercontrol.h"

PJON<SoftwareBitBang> pjonbus_;

// --- PJON ID LIST ---

#define PJON_ID_LIST_LEN 10
uint8_t pjon_id_list_[PJON_ID_LIST_LEN];
uint8_t pjon_id_list_idx_ = 0;

void pjoinidlist_clear()
{
  for (uint8_t c=0; c < PJON_ID_LIST_LEN; c++)
    pjon_id_list_[c] = 0;
  pjon_id_list_idx_ = 0;
}

void pjoinidlist_add(uint8_t id)
{
  if (pjon_id_list_idx_ < PJON_ID_LIST_LEN)
    pjon_id_list_[pjon_id_list_idx_++] = id;
}

int compare_uint8(const void *v1, const void *v2)
{
  return (*((uint8_t*) v1) < *((uint8_t*) v2))? -1 : (*((uint8_t*) v1) > *((uint8_t*) v2))? 1 : 0;
}

void pjoinidlist_sort()
{
  qsort(pjon_id_list_, pjon_id_list_idx_, sizeof(uint8_t), compare_uint8);
}


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
      return sizeof(dampercmd_t)+2;
      break;
    case MSG_PRESSUREINFO:
      return sizeof(pressureinfo_t)+1;
      break;
    case MSG_ERROR:
      return sizeof(errorinfo_t)+1;
      break;
    case MSG_UPDATESETTINGS:
      return sizeof(updatesettings_t)+2;
      break;
    case MSG_PJONID_DOAUTO:
    case MSG_PJONID_QUESTION:
      return 1;
    case MSG_PJONID_INFO:
    case MSG_PJONID_SET:
      return sizeof(pjonidsetting_t)+1;
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

bool pjon_chaincast_didreachall(uint8_t bitfield)
{
  #if NUM_DAMPER != 3
    #error "Code assumes NUM_DAMPER == 3. Rewrite as for-loop"
  #endif
  return ((bitfield & _BV(0)) > 0) && ((bitfield & _BV(1)) > 0) && ((bitfield & _BV(2)) > 0);
}

void pjon_chaincast_recv_handler(uint8_t fromid, pjon_message_t *msg)
{
  //update reach field
  msg->chaincast.reach |= getInstalledDampersAsBitfield();
  bool didreachall = pjon_chaincast_didreachall(msg->chaincast.reach);

  switch(msg->type)
  {
    case MSG_DAMPERCMD:
      printf("got msg_dampercmd\r\n");
      handle_damper_cmd(didreachall, &(msg->chaincast.dampercmd));
      break;
    case MSG_UPDATESETTINGS:
      printf("got MSG_UPDATESETTINGS\r\n");
      updateSettingsFromPacket(&(msg->chaincast.updatesettings));
      break;
    default:
      printf("Unknown MSG type %d\n", msg->type);
      break;
  }

  pjon_chaincast_forward(fromid, didreachall, msg);
}

void pjon_chaincast_forward(uint8_t fromid, bool didreachall, pjon_message_t* msg)
{
  uint8_t next_id = 0;
  if (fromid == 0)
  {
    return; // do not forward broadcasts
  }

  // if pkt has reached µcs controlling all the dampers, forward down-id
  // else forward up-i until bitfield is full
  if (didreachall)
  {
    next_id = pjonbus_.device_id() -1;
  } else {
    next_id = pjonbus_.device_id() +1;
  }

  if (next_id > 0)
  {
    pjonbus_.send(next_id, (char*) msg, pjon_type_to_msg_length(msg->type));
  }
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
    case MSG_UPDATESETTINGS:
      pjon_chaincast_recv_handler(id, (pjon_message_t*) payload);
      break;
    case MSG_PRESSUREINFO:
      printf("MSG_PRESSUREINFO\r\n");
      break;
    case MSG_ERROR:
      printf("MSG_ERROR\r\n");
      break;
    case MSG_PJONID_DOAUTO:
      printf("MSG_PJONID_DOAUTO\r\n");
      pjon_startautoiddiscover();
      break;
    case MSG_PJONID_QUESTION:
      printf("MSG_PJONID_QUESTION\r\n");
      //reply to id with our pjon_id
      pjon_identify_myself(id);
      break;
    case MSG_PJONID_INFO:
      printf("MSG_PJONID_INFO\r\n");
      //save pjon info somewhere
      pjoinidlist_add(id);
      break;
    case MSG_PJONID_SET:
      printf("MSG_PJONID_SET(%d)\r\n",msg->pjonidsetting.pjon_id);
      pjon_change_busid(msg->pjonidsetting.pjon_id);
      break;
    default:
      printf("Unknown MSG type %d\r\n", msg->type);
      break;
  }
}

void pjon_identify_myself(uint8_t toid)
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_INFO;
  msg.pjonidsetting.pjon_id = pjonbus_.device_id();
  pjonbus_.send(toid, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

void pjon_startautoiddiscover()
{
  _delay_ms(200); // let the bus settle after receiving the broadcast
  pjonbus_.acquire_id();
  if (pjonbus_.device_id() == NOT_ASSIGNED)
  {
    _delay_ms(100);
    pjonbus_.acquire_id();
  }
}

void pjon_broadcast_get_autoid()
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_DOAUTO;
  //tell everybody else to get an autoid
  pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
}

void pjon_become_master_of_ids()
{
  pjon_message_t msg;
  msg.type = MSG_PJONID_DOAUTO;
  //become #1
  pjon_change_busid(1);
  //tell everybody else to get an autoid
  pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  uint16_t w;
  for (w=0; w<UINT16_MAX; w++)
    task_pjon();
  //discover id's of everybody else:
  pjoinidlist_clear();
  msg.type = MSG_PJONID_QUESTION;
  pjonbus_.send(BROADCAST, (char*) &msg, pjon_type_to_msg_length(msg.type));
  for (w=0; w<UINT16_MAX; w++)
    task_pjon();  // wait till all µC replied
  // sort id's numerically
  pjoinidlist_sort();
  // for ids
  // first id with gap of >0 to previous id .. tell it to decrease id by gapsize
  // continue until all id's linear 0,1,2,3,4 without gap
  for (uint8_t ii=0; ii<pjon_id_list_idx_; ii++)
  {
    if (pjon_id_list_[ii] != ii+1)
    {
      msg.type = MSG_PJONID_SET;
      msg.pjonidsetting.pjon_id = ii+1;
      if (pjon_id_list_[ii] != msg.pjonidsetting.pjon_id)
        pjonbus_.send(pjon_id_list_[ii], (const char*) &msg, pjon_type_to_msg_length(msg.type));
    }
  }
}

void pjon_inject_msg(uint8_t dst, uint8_t length, uint8_t *payload)
{
  if (dst == 0 || pjonbus_.device_id() == dst)
    pjon_recv_handler(pjon_device_id_, payload, length);
  //hope we did not mangle the payload in recv_handler
  if (dst == 0 || pjonbus_.device_id() != dst)
    pjonbus_.send(dst, (const char*) payload, length);
}

void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload)
{
  pjon_inject_msg(BROADCAST, length, payload);
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
  pjon_device_id_ = id;
  pjonbus_.set_id(pjon_device_id_);
  saveSettings2EEPROM();
}

void pjon_init()
{
  arduino_init();
  pjonbus_.set_error(pjon_error_handler);
  pjonbus_.set_receiver(pjon_recv_handler);
  pjonbus_.set_pin(PIN_PJON);
  if (pjon_device_id_ != NOT_ASSIGNED)
    pjonbus_.set_id(pjon_device_id_);
  pjonbus_.begin();
  if (pjon_device_id_ == NOT_ASSIGNED)
  {
    pjonbus_.acquire_id();
    if (pjonbus_.device_id() != NOT_ASSIGNED)
    {
      pjon_device_id_ = pjonbus_.device_id();
      saveSettings2EEPROM();
    }
  }
}

void task_pjon()
{
    pjonbus_.update();
    pjonbus_.receive(20); 
}