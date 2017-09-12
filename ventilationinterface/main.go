package main

import (
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/btittelbach/pubsub"
)

var (
	ps *pubsub.PubSub
)

const (
	PS_DAMPERSCHANGED       = "damperschanged"
	PS_GETSTATEFORNEWCLIENT = "initalbytes"
	PS_JSONTOALL            = "jsontoall"
)

var (
	DebugFlags_            string
	TeensyTTY_             string
	MinVentChangeInterval_ time.Duration
)

func init() {
	flag.StringVar(&DebugFlags_, "debug", "", "List of debug flags separated by , or ALL")
	flag.StringVar(&TeensyTTY_, "tty", "/dev/ttyACM0", "µC serial device")
	flag.DurationVar(&MinVentChangeInterval_, "mininterval", 1500*time.Millisecond, "Min Invervall between sending cmds to µC")
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
	go goChangeDampers(ps, MinVentChangeInterval_)

	// wait on Ctrl-C or sigInt or sigKill
	func() {
		ctrlc_c := make(chan os.Signal, 1)
		signal.Notify(ctrlc_c, os.Interrupt, os.Kill, syscall.SIGTERM)
		<-ctrlc_c //block until ctrl+c is pressed || we receive SIGINT aka kill -1 || kill
		fmt.Println("SIGINT received, exiting gracefully ...")
		ps.Pub(true, "shutdown")
	}()

}
