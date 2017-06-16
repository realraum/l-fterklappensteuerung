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
	DebugFlags_ string
)

func init() {
	flag.StringVar(&DebugFlags_, "debug", "", "List of DebugFlags separated by ,")
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
	go goChangeDampers(ps)

	// wait on Ctrl-C or sigInt or sigKill
	func() {
		ctrlc_c := make(chan os.Signal, 1)
		signal.Notify(ctrlc_c, os.Interrupt, os.Kill, syscall.SIGTERM)
		<-ctrlc_c //block until ctrl+c is pressed || we receive SIGINT aka kill -1 || kill
		fmt.Println("SIGINT received, exiting gracefully ...")
		ps.Pub(true, "shutdown")
	}()

}
