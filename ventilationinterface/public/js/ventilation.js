"use strict";

var webSocketUrl = 'ws://'+window.location.hostname+'/sock';

var webSocketSupport = null;

var wsctx_ventchange = "ventchange";
var wsctx_error = "error";
var ws_ctx_lock_laser = "locklaser";
var ws_ctx_lock_olga  = "lockolga";

var auth_token_param_name_ = "authtoken";

function handleExternalControlStateChange(data) {
	$(".controlstate").removeClass("active");
	$(".lockbutton").removeClass("active");
	Object.keys(data).forEach(function(name)  {
		if (data[name]) {
			$(".controlstate[name="+name+"][state="+data[name]+"]").addClass("active");
			$(".lockbutton[name="+name+"]").addClass("active");
		}
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

function handleLockButtonClick(event) {
	if ($(event.target).attr("name")!="OLGALock" )
		return; //only handle OLGALock Button Changes
	if ($(event.target).hasClass("active"))
	{
		$(event.target).removeClass("active");
	} else {
		$(event.target).addClass("active");
	}
	sendOLGALockUpdate();
}

function displayErrorMsg(msg, duration) {
	$("#error-msg").text(msg);
	$("div.erroroverlay").css("display","table");
	setTimeout(function() {$("div.erroroverlay").css("display","none");}, duration);
}

function handleErrorMsg(data) {
	displayErrorMsg(data.msg, 1100);
}

function ShowWaitingForConnection() {
    $("div.waitingoverlay").css("display","initial");
}

function ShowConnectionEstablished() {
	$("div.waitingoverlay").css("display","none");
}

function getLocationURLParameter(queryparam) {
	var urlParams = new URLSearchParams(window.location.search);
	if (urlParams.has(queryparam))
		return urlParams.get(queryparam);
	else
		return "";
}

function sendOLGALockUpdate() {
	var newstate=$(".lockbutton[name=\"OLGALock\"]").hasClass("active");
	var local_auth_token=getLocationURLParameter(auth_token_param_name_);
	ws.send(ws_ctx_lock_olga, {"olga":newstate,"authtoken":local_auth_token});
}

(function() {
  webSocketSupport = hasWebSocketSupport();

  //set background color for fancylightpresetbuttons according to ledr=, ledb=, etc.
  if (webSocketSupport) {
	$(".controlstate").on("click",handleControlStateChange);
	$(".lockbutton").on("click",handleLockButtonClick);

    ws.registerContext(wsctx_ventchange,handleExternalControlStateChange);
    ws.registerContext(wsctx_error,handleErrorMsg);
    ws.onopen = ShowConnectionEstablished;
    ws.ondisconnect = ShowWaitingForConnection;
    ws.open(webSocketUrl);
  }
})();
