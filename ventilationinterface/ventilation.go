package main

import (
	"encoding/json"
	"time"

	"github.com/btittelbach/pubsub"
)

type DamperRequest struct {
	islocal       bool
	toclient_chan chan []byte
	request       wsChangeVent
}

func sanityCheckRequestedVentilationState(state *wsChangeVent) bool {
	switch state.Damper1 {
	case ws_damper_state_closed, ws_damper_state_half, ws_damper_state_open:
	default:
		return false
	}
	switch state.Damper2 {
	case ws_damper_state_closed, ws_damper_state_half, ws_damper_state_open:
	default:
		return false
	}
	switch state.Damper3 {
	case ws_damper_state_closed, ws_damper_state_half, ws_damper_state_open:
	default:
		return false
	}
	switch state.Fan {
	case ws_fan_state_off, ws_fan_state_on:
	default:
		return false
	}
	return true
}

func sanityCheckVentilationStateChange(prev_state, new_state *wsChangeVent, local_request, lasercutter_in_use, olga_locked bool) *wsError {
	closing_damper1 := prev_state.Damper1 != new_state.Damper1 && new_state.Damper1 == ws_damper_state_closed
	opening_damper1 := prev_state.Damper1 != new_state.Damper1 && new_state.Damper1 != ws_damper_state_closed
	closing_damper2 := prev_state.Damper2 != new_state.Damper2 && new_state.Damper2 == ws_damper_state_closed
	opening_damper2 := prev_state.Damper2 != new_state.Damper2 && new_state.Damper2 == ws_damper_state_open
	closing_damper3 := prev_state.Damper3 != new_state.Damper3 && new_state.Damper3 == ws_damper_state_closed
	opening_damper3 := prev_state.Damper3 != new_state.Damper3 && new_state.Damper3 == ws_damper_state_open
	stopping_fan := prev_state.Fan != new_state.Fan && new_state.Fan == ws_fan_state_off
	starting_fan := prev_state.Fan != new_state.Fan && new_state.Fan == ws_fan_state_on
	all_dampers_closed := new_state.Damper1 == ws_damper_state_closed && new_state.Damper2 == ws_damper_state_closed && new_state.Damper3 == ws_damper_state_closed
	all_dampers_opened := new_state.Damper1 != ws_damper_state_closed && new_state.Damper2 != ws_damper_state_closed && new_state.Damper3 != ws_damper_state_closed

	if starting_fan && all_dampers_closed {
		return &wsError{Type: ws_error_prohibited, Msg: "Won't start Fan with dampers closed!"}
	}
	if new_state.Fan == ws_fan_state_on && all_dampers_opened && !local_request {
		return &wsError{Type: ws_error_notauth, Msg: "Please open only 2 dampers at a time"}
	}
	if lasercutter_in_use {
		if closing_damper1 {
			return &wsError{Type: ws_error_prohibited, Msg: "Can't close LaserDamper while Lasercutter in use!"}
		}
		if opening_damper2 || opening_damper3 {
			return &wsError{Type: ws_error_prohibited, Msg: "Can't fully open OLGA dampers while Lasercutter in use!"}
		}
		if stopping_fan {
			return &wsError{Type: ws_error_prohibited, Msg: "Can't stop fan while Lasercutter in use!"}
		}
	}
	if olga_locked && !local_request {
		if new_state.Fan == ws_fan_state_off {
			return &wsError{Type: ws_error_notauth, Msg: "Can't stop fan while OLGA locked it"}
		}
		if closing_damper2 || closing_damper3 || opening_damper1 {
			return &wsError{Type: ws_error_notauth, Msg: "Can't close OLGA dampers or open LaserDamper while OLGA needs ventilation"}
		}
	}
	//ensure fan is off if all dampers closed
	if all_dampers_closed {
		new_state.Fan = ws_fan_state_off
	}
	return nil
}

func goSanityCheckDamperRequests(ps *pubsub.PubSub, locktimeout time.Duration) {
	newreq_c := ps.Sub(PS_DAMPERREQUEST)
	newlock_c := ps.Sub(PS_LOCKCHREQ)
	shutdown_c := ps.SubOnce("shutdown")
	defer ps.Unsub(newlock_c, PS_LOCKCHREQ)
	defer ps.Unsub(newreq_c, PS_DAMPERREQUEST)
	var last_state wsChangeVent = wsChangeVent{Damper1: ws_damper_state_closed, Damper2: ws_damper_state_closed, Damper3: ws_damper_state_closed, Fan: ws_fan_state_off}
	var know_last_state bool = false
	var OLGALock bool = false
	var LaserLock bool = false
	locktimeouter := time.NewTimer(0)

	for {
		select {
		case <-shutdown_c:
			return
		case newlock_i := <-newlock_c:
			switch newlock := newlock_i.(type) {
			case wsLockChangeLaser:
				LaserLock = newlock.LaserLock
				if newlock.LaserLock {
					locktimeouter.Reset(locktimeout)
				}
			case wsLockChangeOLGA:
				OLGALock = newlock.OLGALock
				if newlock.OLGALock {
					locktimeouter.Reset(locktimeout)
				}
			default:
			}
		case <-locktimeouter.C:
			LaserLock = false
			OLGALock = false
			last_state.OLGALock = OLGALock
			last_state.LaserLock = LaserLock
			ps.Pub(last_state, PS_DAMPERSCHANGED)
		case newreq_i := <-newreq_c:
			var wserr *wsError = nil

			newreq := newreq_i.(DamperRequest)
			LogVent_.Print("CmdFromWeb:", newreq)
			if know_last_state {
				wserr = sanityCheckVentilationStateChange(&last_state, &newreq.request, newreq.islocal, LaserLock, OLGALock)
			}
			if wserr == nil {
				last_state = newreq.request
				last_state.OLGALock = OLGALock
				last_state.LaserLock = LaserLock
				ps.Pub(last_state, PS_DAMPERSCHANGED)
				know_last_state = true
			} else {
				if newreq.toclient_chan != nil {
					if know_last_state {
						replydata, err := json.Marshal(wsMessageOut{Ctx: ws_ctx_ventchange, Data: last_state})
						if err != nil {
							LogWS_.Print(err)
							continue
						}
						select { //make sure we don't crash or hang by writing to closed or blocked chan
						case newreq.toclient_chan <- replydata:
						default:
						}
					}
					replydata, err := json.Marshal(wsMessageOut{Ctx: ws_ctx_error, Data: *wserr})
					if err != nil {
						LogWS_.Print(err)
						continue
					}
					select { //make sure we don't crash or hang by writing to closed or blocked chan
					case newreq.toclient_chan <- replydata:
					default:
					}
				}
			}
		}
	}
}
