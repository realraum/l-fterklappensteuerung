"use strict";

var webSocketUrl = 'ws://'+window.location.hostname+'/sock';

var webSocketSupport = null;

var wsctx_ventchange = "ventchange";

function handleExternalControlStateChange(data) {
	$(".controlstate").removeClass("active");
	Object.keys(data).forEach(function(name)  {
		$(".controlstate[name="+name+"][state="+data[name]+"]").addClass("active");
	});
}

function sendControlStateUpdate() {
	var newstate={};
	$(".controlstate.active").each(function(elem){
		newstate[elem.getAttribute("name")]=elem.getAttribute("state");
	});
	ws.send(wsctx_ventchange, newstate);
}

function handleControlStateChange(event) {
	var mygrp = $(event.target).parent().children().removeClass("active");
	$(event.target).addClass("active");
	sendControlStateUpdate();
}

(function() {
  webSocketSupport = hasWebSocketSupport();

  //set background color for fancylightpresetbuttons according to ledr=, ledb=, etc.
  if (webSocketSupport) {
	$(".controlstate").on("click",handleControlStateChange);

    ws.registerContext(wsctx_ventchange,handleExternalControlStateChange);
    ws.open(webSocketUrl);
  }
})();
