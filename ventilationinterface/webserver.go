package main

import (
	"encoding/json"
	"net/http"
	"time"

	"github.com/btittelbach/pubsub"
	"github.com/codegangsta/martini"
	"github.com/gorilla/websocket"
	"github.com/mitchellh/mapstructure"
)

const (
	ws_ctx_ventchange      = "ventchange"
	ws_damper_state_closed = "closed"
	ws_damper_state_half   = "halfopen"
	ws_damper_state_open   = "open"
	ws_fan_state_on        = "on"
	ws_fan_state_off       = "off"
)

type wsMessage struct {
	Ctx  string                 `json:"ctx"`
	Data map[string]interface{} `json:"data"`
}

type wsMessageOut struct {
	Ctx  string      `json:"ctx"`
	Data interface{} `json:"data"`
}

type wsChangeVent struct {
	Damper1 string
	Damper2 string
	Damper3 string
	Fan     string
}

const (
	ws_ping_period_      = time.Duration(58) * time.Second
	ws_read_timeout_     = time.Duration(70) * time.Second // must be > than ws_ping_period_
	ws_write_timeout_    = time.Duration(9) * time.Second
	ws_max_message_size_ = int64(512)
	http_read_timeout_   = time.Duration(5) * time.Second // must be > than ws_ping_period_
	http_write_timeout_  = time.Duration(10) * time.Second
)

var wsupgrader = websocket.Upgrader{} // use default options with Origin Check

func wsWriteMessage(ws *websocket.Conn, mt int, data []byte) error {
	ws.SetWriteDeadline(time.Now().Add(ws_write_timeout_))
	if err := ws.WriteMessage(mt, data); err != nil {
		LogWS_.Println("wsWriteTextMessage", ws.RemoteAddr(), "Error", err)
		return err
	}
	return nil
}

func goMarshalJsonForAllClients(ps *pubsub.PubSub) {
	shutdown_c := ps.SubOnce("shutdown")
	udpate_c := ps.Sub(PS_DAMPERSCHANGED)
	defer ps.Unsub(udpate_c, PS_DAMPERSCHANGED)
	init_c := ps.Sub(PS_GETSTATEFORNEWCLIENT)
	defer ps.Unsub(init_c, PS_GETSTATEFORNEWCLIENT)
	var initial_info []byte = []byte("{\"ctx\":\"" + ws_ctx_ventchange + "\",\"data\":{}}")
	for {
		select {
		case <-shutdown_c:
			return
		case future_chan_i := <-init_c:
			future_chan_i.(chan []byte) <- initial_info
		case damperinfo := <-udpate_c:
			replydata, err := json.Marshal(wsMessageOut{Ctx: ws_ctx_ventchange, Data: damperinfo})
			if err != nil {
				LogWS_.Print(err)
				continue
			}
			initial_info = replydata
			ps.Pub(replydata, PS_JSONTOALL)
		}
	}
}

func goWriteToClient(ws *websocket.Conn, toclient_chan chan []byte, ps *pubsub.PubSub) {
	shutdown_c := ps.SubOnce("shutdown")
	udpate_c := ps.Sub(PS_JSONTOALL)
	ticker := time.NewTicker(ws_ping_period_)
	defer ps.Unsub(udpate_c, PS_JSONTOALL)

	initdata_c := make(chan []byte, 2)
	ps.Pub(initdata_c, PS_GETSTATEFORNEWCLIENT)

WRITELOOP:
	for {
		var err error
		select {
		case <-shutdown_c:
			LogWS_.Println("goWriteToClient", ws.RemoteAddr(), "Shutdown")
			break WRITELOOP
		case jsonbytes, isopen := <-udpate_c:
			if !isopen {
				break WRITELOOP
			}
			err = wsWriteMessage(ws, websocket.TextMessage, jsonbytes.([]byte))
		case initbytes := <-initdata_c:
			err = wsWriteMessage(ws, websocket.TextMessage, initbytes)
		case replybytes, isopen := <-toclient_chan:
			if !isopen {
				break WRITELOOP
			}
			err = wsWriteMessage(ws, websocket.TextMessage, replybytes)
		case <-ticker.C:
			err = wsWriteMessage(ws, websocket.PingMessage, []byte{})
		}
		if err != nil {
			return
		}
	}
	wsWriteMessage(ws, websocket.CloseMessage, []byte{})
}

func goTalkWithClient(w http.ResponseWriter, r *http.Request, ps *pubsub.PubSub) {
	ws, err := wsupgrader.Upgrade(w, r, nil)
	if err != nil {
		LogWS_.Println(err)
		return
	}
	// client := ws.RemoteAddr()
	LogWS_.Println("Client connected", ws.RemoteAddr())

	// NOTE: no call to ws.WriteMessage in this function after this call
	// ONLY goWriteToClient writes to client
	toclient_chan := make(chan []byte, 10)
	defer close(toclient_chan)
	go goWriteToClient(ws, toclient_chan, ps)

	// logged_in := false
	ws.SetReadLimit(ws_max_message_size_)
	ws.SetReadDeadline(time.Now().Add(ws_read_timeout_))
	// the PongHandler will set the read deadline for next messages if pings arrive
	ws.SetPongHandler(func(string) error { ws.SetReadDeadline(time.Now().Add(ws_read_timeout_)); return nil })
WSREADLOOP:
	for {
		var v wsMessage
		err := ws.ReadJSON(&v)
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway) {
				LogWS_.Printf("webHandleWebSocket Error: %v", err)
			}
			break
		}

		switch v.Ctx {
		case ws_ctx_ventchange:
			var data wsChangeVent
			err = mapstructure.Decode(v.Data, &data)
			if err != nil {
				LogWS_.Printf("%s Data decode error: %s", v.Ctx, err)
				continue WSREADLOOP
			}
			ps.Pub(data, PS_DAMPERSCHANGED)
		}
	}
}

func ListenAndServeWithSaneTimeouts(addr string, handler http.Handler) error {
	server := &http.Server{Addr: addr, Handler: handler, ReadTimeout: http_read_timeout_, WriteTimeout: http_write_timeout_}
	return server.ListenAndServe()
}

func RunMartini(ps *pubsub.PubSub) {
	go goMarshalJsonForAllClients(ps)
	m := martini.Classic()
	//m.Use(martini.Static("/var/lib/cloud9/static/"))
	m.Get("/sock", func(w http.ResponseWriter, r *http.Request) {
		goTalkWithClient(w, r, ps)
	})

	if err := ListenAndServeWithSaneTimeouts(":80", m); err != nil {
		LogWS_.Fatal(err)
	}

}
