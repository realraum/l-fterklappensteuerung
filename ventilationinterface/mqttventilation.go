package main

import (
	"encoding/json"
	"time"

	"github.com/btittelbach/pubsub"
	mqtt "github.com/eclipse/paho.mqtt.golang"
	"github.com/realraum/door_and_sensors/r3events"
)

type mqttChangeVent struct {
	wsChangeVent
	Ts int64
}

const TIMEOUT_OFF_AFTER_EVERYBODY_LEFT = 2 * time.Minute

//Connect and keep trying to connect to MQTT Broker
//And while we cannot, still provide as much functionality as possible
func goConnectToMQTTBrokerAndFunctionWithoutInTheMeantime(ps *pubsub.PubSub) {
	//try connect to mqtt daemon until it works once
	for {
		mqttc := ConnectMQTTBroker(MQTTBroker_, MQTTClientID_)
		//start real goroutines after mqtt connected
		if mqttc != nil {
			SubscribeAndAttachCallback(mqttc, r3events.TOPIC_LASER_CARD, func(c mqtt.Client, msg mqtt.Message) {
				var lp r3events.LaserCutter
				if err := json.Unmarshal(msg.Payload(), &lp); err == nil {
					ps.Pub(lp.IsHot, PS_LOCKUPDATES)
				}
			})
			go func() {
				shutdown_c := ps.SubOnce(PS_SHUTDOWN)
				mqtt_presence_chan := SubscribeAndForwardToChannel(mqttc, r3events.TOPIC_META_PRESENCE)
				off_after_presence_false_timeout := time.NewTimer(TIMEOUT_OFF_AFTER_EVERYBODY_LEFT)
				if !off_after_presence_false_timeout.Stop() {
					<-off_after_presence_false_timeout.C //in case TIMEOUT_OFF_AFTER_EVERYBODY_LEFT == 0
				}
				people_present := true

				for {
					select {
					case msg := <-mqtt_presence_chan:
						var lp r3events.PresenceUpdate
						if err := json.Unmarshal(msg.Payload(), &lp); err == nil {
							people_present = lp.Present
							if people_present == false {
								off_after_presence_false_timeout.Reset(TIMEOUT_OFF_AFTER_EVERYBODY_LEFT)
							}
						}
					case <-off_after_presence_false_timeout.C:
						if people_present == false {
							ps.Pub(DamperRequest{request: wsChangeVent{Damper1: ws_damper_state_closed, Damper2: ws_damper_state_closed, Damper3: ws_damper_state_closed, Fan: ws_fan_state_off}, islocal: true, toclient_chan: nil}, PS_DAMPERREQUEST)
						}
					case <-shutdown_c:
						return
					}
				}
			}()

			go func() {
				newstate_c := ps.Sub(PS_DAMPERSCHANGED)
				shutdown_c := ps.SubOnce(PS_SHUTDOWN)
				defer ps.Unsub(newstate_c, PS_DAMPERSCHANGED)

				for {
					select {
					case <-shutdown_c:
						return
					case newstate := <-newstate_c:
						payload := mqttChangeVent{wsChangeVent: newstate.(wsChangeVent), Ts: time.Now().Unix()}
						mqttc.Publish(r3events.TOPIC_VENTILATION, 0, false, r3events.MarshalEvent2ByteOrPanic(payload))
					}
				}
			}()
			//and LAST but not least:
			return // no need to keep on trying, mqtt-auto-reconnect will do the rest now
		} else {
			time.Sleep(5 * time.Minute)
		}
	}
}
