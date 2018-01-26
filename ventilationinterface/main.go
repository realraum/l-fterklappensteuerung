package main

import (
	"flag"
	"fmt"
	"math/rand"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/btittelbach/pubsub"
	"github.com/realraum/door_and_sensors/r3events"
)

var (
	ps *pubsub.PubSub
)

const (
	PS_DAMPERSCHANGED       = "damperschanged"
	PS_DAMPERREQUEST        = "damperrequest"
	PS_GETSTATEFORNEWCLIENT = "initalbytes"
	PS_JSONTOALL            = "jsontoall"
	PS_SHUTDOWN             = "shutdown"
)

var (
	LocalAuthToken_               string
	DebugFlags_                   string
	TeensyTTY_                    string
	MinVentChangeInterval_        time.Duration
	MQTTBroker_                   string
	MQTTClientID_                 string
	LockTimeout_                  time.Duration
	OffAfterEverybodyLeftTimeout_ time.Duration
)

func init() {
	rand.Seed(time.Now().UnixNano())
	flag.StringVar(&MQTTBroker_, "mqttbroker", "tcp://mqtt.realraum.at:1883", "MQTT Broker")
	flag.StringVar(&MQTTClientID_, "mqttclientid", r3events.CLIENTID_VENTILATION, "MQTT Client ID")
	flag.StringVar(&LocalAuthToken_, "localtoken", "", "Token provided by website so we know its from the local touch display")
	flag.StringVar(&DebugFlags_, "debug", "", "List of debug flags separated by , or ALL")
	flag.StringVar(&TeensyTTY_, "tty", "/dev/ttyACM0", "µC serial device")
	flag.DurationVar(&MinVentChangeInterval_, "mininterval", 1500*time.Millisecond, "Min Invervall between sending cmds to µC")
	flag.DurationVar(&LockTimeout_, "locktimeout", 30*time.Minute, "Timeout for OLGA/Lasercutter Lock")
	flag.DurationVar(&OffAfterEverybodyLeftTimeout_, "autoofftimeout", 2*time.Minute, "Timeout for automatic Off after everybody left")
}

func main() {
	flag.Parse()
	if len(DebugFlags_) > 0 {
		LogEnable(strings.Split(DebugFlags_, ",")...)
	}
	ps = pubsub.New(10)

	// call rest of main is submain func, thus give submain() defers time to do their work @exit
	MainThatReallyIsTheRealMain()

	LogMain_.Print("Exiting..")
	time.Sleep(100 * time.Millisecond) //give goroutines time to shutdown, so we can safely unmap mmaps
}

func MainThatReallyIsTheRealMain() {
	defer ps.Pub(true, "shutdown")

	go RunMartini(ps)
	go goSanityCheckDamperRequests(ps, LockTimeout_)
	go goChangeDampers(ps, MinVentChangeInterval_)
	go goConnectToMQTTBrokerAndFunctionWithoutInTheMeantime(ps)

	// wait on Ctrl-C or sigInt or sigKill
	func() {
		ctrlc_c := make(chan os.Signal, 1)
		signal.Notify(ctrlc_c, os.Interrupt, os.Kill, syscall.SIGTERM)
		<-ctrlc_c //block until ctrl+c is pressed || we receive SIGINT aka kill -1 || kill
		fmt.Println("SIGINT received, exiting gracefully ...")
		ps.Pub(true, PS_SHUTDOWN)
	}()

}
