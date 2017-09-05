package main

import "github.com/btittelbach/pubsub"

type SerialLine []byte

/**


enum pjon_msg_type_t {MSG_DAMPERCMD, MSG_PRESSUREINFO, MSG_ERROR, MSG_UPDATESETTINGS, MSG_PJONID_DOAUTO, MSG_PJONID_QUESTION, MSG_PJONID_INFO, MSG_PJONID_SET};
enum damper_cmds_t {DAMPER_CLOSED, DAMPER_OPEN, DAMPER_HALFOPEN};
enum fan_cmds_t {FAN_OFF=0, FAN_ON=1};
enum error_type_t {NO_ERROR, DAMPER_CONTROL_TIMEOUT};


typedef struct {
  uint8_t damper[NUM_DAMPER];
  uint8_t fan : 1;
  uint8_t fanlamina : 1;
} dampercmd_t;

**/

const (
	damperteensy_type_dampercmd     uint8 = 0
	damperteensy_cmd_damperclosed   uint8 = 0
	damperteensy_cmd_damperopen     uint8 = 1
	damperteensy_cmd_damperhalfopen uint8 = 2
	damperteensy_cmd_fanon          uint8 = 1
	damperteensy_cmd_fanoff         uint8 = 0
)

var damperteensy_cmdmap map[string]uint8 = map[string]uint8{ws_damper_state_closed: damperteensy_cmd_damperclosed, ws_damper_state_open: damperteensy_cmd_damperopen, ws_damper_state_half: damperteensy_cmd_damperhalfopen, ws_fan_state_off: damperteensy_cmd_fanoff, ws_fan_state_on: damperteensy_cmd_fanon}

func mkDamperCmdMsg(newstate wsChangeVent) []byte {
	buf := make([]byte, 10)
	i := 0
	inmap := false
	buf[i] = damperteensy_type_dampercmd //msg type
	i++
	buf[i] = 0 //reach
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper1] //Damper[0]
	if inmap == false {
		return nil
	}
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper2] //Damper[1]
	if inmap == false {
		return nil
	}
	i++
	buf[i], inmap = damperteensy_cmdmap[newstate.Damper3] //Damper[2]
	if inmap == false {
		return nil
	}
	i++
	buf[i] = damperteensy_cmdmap[newstate.Fan] // Laminaflow Fan
	if inmap == false {
		return nil
	}
	buf[i] |= damperteensy_cmdmap[newstate.Fan] << 1 // Fan
	if inmap == false {
		return nil
	}
	i++
	return buf[0:i]
}

func goChangeDampers(ps *pubsub.PubSub) {
	newstate_c := ps.Sub(PS_DAMPERSCHANGED)
	shutdown_c := ps.SubOnce("shutdown")
	defer ps.Unsub(newstate_c, PS_DAMPERSCHANGED)
	teensytty_wr, teensytty_rd, teensytty_err := OpenAndHandleSerial(TeensyTTY_, 9600)
	if teensytty_err != nil {
		panic(teensytty_err)
	}

	for {
		select {
		case <-shutdown_c:
			return
		case newstate := <-newstate_c:
			LogVent_.Print("CmdFromWeb:", newstate.(wsChangeVent))
			cmdbytes := mkDamperCmdMsg(newstate.(wsChangeVent))
			if cmdbytes != nil && len(cmdbytes) > 0 {
				LogVent_.Print("ToPJON:", cmdbytes)
				teensytty_wr <- cmdbytes
			}
		case line := <-teensytty_rd:
			LogVent_.Print("FromPJON:", line)
		}
	}
}
