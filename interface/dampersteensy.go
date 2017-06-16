package main

import "github.com/btittelbach/pubsub"

func goChangeDampers(ps *pubsub.PubSub) {
	newstate_c := ps.Sub(PS_DAMPERSCHANGED)
	shutdown_c := ps.SubOnce("shutdown")
	defer ps.Unsub(newstate_c, PS_DAMPERSCHANGED)

	for {
		select {
		case <-shutdown_c:
			return
		case newstate := <-newstate_c:
			LogVent_.Print(newstate.(wsChangeVent))
		}
	}

}
