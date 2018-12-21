var VLV = "valve_";
var VLVP = VLV + "path_";
var VLVT = VLV + "text_";
var ON = "ON";
var OFF = "OFF";
var OFFL = "OFFLINE";
var GRN = "#00D800";
var RED = "#FF0000";
var status = "status";

$(document).ready(init);

function init(){
	$(document.body).addClass("unselectable");
	
	resizeAllValves();
	
	document.addEventListener("click", function(){
		var e = event.srcElement;

		var valve = findValve(e);
		
		if (valve != null){
			var valveno = $(valve).attr("id").replace(VLV, "");
			toggleValve(valveno);
		}
	});
}

function resizeAllValves(){
	var valves = document.getElementsByClassName("valve");
	var offsetWidth = document.body.offsetWidth;
	var offsetWidthPart = offsetWidth / 5.3; //4 valves per row
	
	for (var i=0; i<valves.length; i++){
		valves[i].style.zoom = offsetWidthPart + "%";
	}

}

function findValve(e){
	if (e.className!="valve"){
		while (e.tagName!="BODY"){
			e = e.parentNode;
			if (e.tagName == "DIV"){
				//found a div
				if (e.className == "valve"){
					//found a valve
					return e;
				}
			}
		}
		return null;
	}else{
		return e;
	}
}

function toggleValve(valveno){
	
	var e = document.getElementById(VLV + valveno);
			
	if ($(e).attr(status)==OFFL){
		return;
	}else if ($(e).attr(status)==ON){
		sendRequest(valveno, "off");
	}else{
		sendRequest(valveno, "on");
			
	}
}

function sendRequest(pos, state){
	$.ajax({
		method: "GET",
		url: "action",
		data: { valveno: pos, valvestatus: state }
	})
	.done(function( msg ) {
			
		var obj = JSON.parse(msg);
		var valves = obj.valves;
				
		//loop through response valves and do stuff
		for (var i=0; i<valves.length; i++){
			
			var valveno = valves[i].no;
			var valvestatus = valves[i].status;
			var valvelabel = valves[i].label;

			var fillcolor;
						
			var e = $("#" + VLV + valveno);
			var p = $("#" + VLVP + valveno);
			var t = $("#" + VLVT + valveno);
						
			if (valvestatus==ON){
				fillcolor = GRN;
			}else if (valvestatus==OFF){
				fillcolor = RED;
			}else{
				continue;
			}
			
			$(p).attr("fill", fillcolor);
			$(t).html(valvelabel + ": " + valvestatus);
			$(e).attr(status, valvestatus);
		}
	});
}