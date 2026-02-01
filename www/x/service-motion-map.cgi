#!/bin/haserl
<%in _common.cgi %>
<%
page_title="Motion Guard/Map"
%>
<%in _header.cgi %>

<% field_switch "motion_enabled" "Enable motion guard" %>
<div class="row row-cols-xl-2 row-cols-md-2">
<div style="margin-bottom: 0px;">
<div class="row m-2">
<div class="col-6"> <% field_range "motion_sensitivity" "Sensitivity" "1,4,1" %> </div>
<div class="col-6"> <% field_range "motion_cooldown_time" "Delay between alerts" "1,60,1" %></div>
<div class="row l-3">
<div class="col-4"><% field_switch "motion_mapMode" "Map Mode" %></div>
<div class="col-4"><% field_switch "motion_tracking_enable" "Auto Tracking" %></div>
<div class="col-4"><% field_switch "motion_autoHome" "Auto Home" %></div>
<div class="col-6"><% field_number "motion_post_time" "Light off delay (sec)" %> </div>
<div class="col-6"><% field_number "motion_home_time" "Home delay (sec)" %> </div>
</div>

<div id="motion-map" class="position-relative ">
<div id="motion-map" class="canvas-container" style="border: 2px solid #889988;">
	<img id="preview" src="/x/ch1.mjpg" class="img-fluid" alt="Image: Stream Preview">
    	<canvas id="tileOnImage"></canvas>
    	<canvas id="gridOnImage"></canvas>
    	<canvas id="imageCanvas"></canvas>
	<div id="boxOnImage" class="overlayLayer"></div>
<style>

.canvas-container {
    position: relative;
    display: inline-block;
}

.overlayLayer{
	display: none;
	position: absolute;
	border:2px solid #ff6666;
	background-color:transparent
    	top: 0;
    	left: 0;
}

#tileOnImage{
    position: absolute;
    top: 0;
    left: 0;
    width: 100%; 
    height: 100%;
    border: 1px solid black; /* Optional: adds a border to the canvas area */
}

#gridOnImage{
    position: absolute;
    top: 0;
    left: 0;
    width: 100%; 
    height: 100%;
    border: 1px solid black; /* Optional: adds a border to the canvas area */
}

#imageCanvas {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%; 
    height: 100%;
    border: 1px solid black; /* Optional: adds a border to the canvas area */
}
</style>
</div>

<div id="motion-status">
   ROI Box - X0: <span id="motion_roi_0_x"></span>
   , Y0: <span id="motion_roi_0_y"></span>
   - X1: <span id="motion_roi_1_x"></span>
   , Y1: <span id="motion_roi_1_y"></span>
</div>

<div class="row m-2">
<div class="col-5"> <button type="button" class="btn btn-secondary" id="clear_ROI">Clear All ROIs</button></div>
<!--<button type="button" class="btn btn-secondary" id="request-rois">Get ROIs</button>-->
<div class="col-5"><button type="button" class="btn btn-secondary" id="saveMAP-rois">Save ROIs</button></div>
</div>
</div>
</div>
</div>

<div class="col">
<% field_checkbox "motion_whiteLight" "Turn on white light" "" %>
<hr>
<% field_checkbox "motion_send2email" "Send to email address" "<a href=\"tool-send2email.cgi\">Configure sending to email</a>" %>
<% field_checkbox "motion_send2telegram" "Send to Telegram" "<a href=\"tool-send2telegram.cgi\">Configure sending to Telegram</a>" %>
<% field_checkbox "motion_send2mqtt" "Send to MQTT" "<a href=\"tool-send2mqtt.cgi\">Configure sending to MQTT</a>" %>
<% field_checkbox "motion_send2webhook" "Send to webhook" "<a href=\"tool-send2webhook.cgi\">Configure sending to a webhook</a>" %>
<% field_checkbox "motion_send2ftp" "Upload to FTP" "<a href=\"tool-send2ftp.cgi\">Configure uploading to FTP</a>" %>
<% field_checkbox "motion_send2ntfy" "Send to Ntfy" "<a href=\"tool-send2ntfy.cgi\">Configure sending to Ntfy</a>" %>

<div class="alert alert-info">
<p>A motion event is detected by the streamer which triggers the <code>/sbin/motion</code> script,
which sends alerts through the selected and preconfigured notification methods.</p>
<% wiki_page "Plugin:-Motion-Guard" %>
</div>
</div>
</div>

<%in _preview.cgi %>

<script>
const endpoint = '/x/json-prudynt.cgi';
const send2_targets = ['email', 'ftp', 'mqtt', 'telegram', 'webhook', 'ntfy'];
// motion
const motion_params = ['enabled', 'sensitivity', 'cooldown_time', 'whiteLight', 'mapMode', 'post_time', 'home_time', 'tracking_enable', 'autoHome'];
const roi_params =['roi_0_x', 'roi_0_y', 'roi_1_x', 'roi_1_y'];


function handleMessage(msg) {
	if (msg.action && msg.action.capture == 'initiated') return;

	let data;

	data = msg.motion;
	if (data) {
		if (data.enabled)
			$('#motion_enabled').checked = data.enabled;
		if (data.tracking_enable)
			$('#motion_tracking_enable').checked = data.tracking_enable;
		if (data.autoHome)
			$('#motion_autoHome').checked = data.autoHome;
		if (data.whiteLight)
			$('#motion_whiteLight').checked = data.whiteLight;
		if (data.mapMode !== undefined) {
			$('#motion_mapMode').checked = data.mapMode;
			if($('#motion_mapMode').checked) {
    				gridOnImage.style.visibility='visible';
    				tileOnImage.style.visibility='visible';
			} else {
    				gridOnImage.style.visibility='hidden';
    				tileOnImage.style.visibility='hidden';
			}
		}
		if (data.post_time) {
			$('#motion_post_time').value = data.post_time;
		}
		if (data.home_time) {
			$('#motion_home_time').value = data.home_time;
		}
		if (data.sensitivity) {
			$('#motion_sensitivity').value = data.sensitivity;
			$('#motion_sensitivity-show').textContent = data.sensitivity;
		}
		if (data.cooldown_time) {
			$('#motion_cooldown_time').value = data.cooldown_time;
			$('#motion_cooldown_time-show').textContent = data.cooldown_time;
		}
		if (data.rois) {
			// console.log(data.rois);
			if (Array.isArray(data.rois)) {
				init_tiles(false);
				parse_ROI(data.rois);
			}
		}
		roi_params.forEach((x) => {
			if (typeof(data[x]) !== 'undefined') {
				setValue(data, 'motion', x);
				($(`#motion_${x}`)).textContent = data[x];
				($(`#motion_${x}`)).value = data[x];
			}
    			ctx.strokeStyle = 'rgba(0,255,0, 0.4)'; // green, transparent
			drawROIbox();
		});
	}
}

async function loadConfig() {
	const payload =	'{"motion":{' + motion_params.map((x) => `"${x}":null`).join() + '}}';

	console.log('===>', payload);
	try {
		const response = await fetch(endpoint, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: payload
		});
		if (!response.ok) throw new Error('HTTP ${response.status}');
		const contentType = response.headers.get('content-type');
		if (contentType?.includes('application/json')) {
			const msg = await response.json();
			console.log(ts(), '<===', JSON.stringify(msg));
			handleMessage(msg);
		}
	} catch (err) {
		console.error('Load config error', err);
	}
}

async function loadROI() {
	const payload =	'{"motion":{' + roi_params.map((x) => `"${x}":null`).join() + '}}';

	console.log('===>', payload);
	try {
		const response = await fetch(endpoint, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: payload
		});
		if (!response.ok) throw new Error('HTTP ${response.status}');
		const contentType = response.headers.get('content-type');
		if (contentType?.includes('application/json')) {
			const msg = await response.json();
			console.log(ts(), '<===', JSON.stringify(msg));
			handleMessage(msg);
		}
	} catch (err) {
		console.error('Load config error', err);
	}
}

motion_params.forEach((x) => {
	$(`#motion_${x}`).onchange = () => saveValue('motion', x);
});

async function switchSend2Target(target, state) {
	await fetch('/x/json-motion.cgi?' + new URLSearchParams({ "target": target, "state": state }).toString())
		.then(res => res.json())
		.then(data => { $('#motion_send2${data.message.target}').checked = (data.message.status == 1) });
}

send2_targets.forEach((x) => {
	$(`#motion_send2${x}`).onchange = (ev) => switchSend2Target(x, ev.target.checked);
});

//$("#motion_whiteLight").addEventListener('change', ev =>
//        sendToEndpoint('{"motion":{"whiteLight":'+ev.target.checked+'}}')); 

//$("#motion_mapMode").addEventListener('change', ev =>
//        sendToEndpoint('{"motion":{"mapMode":'+ev.target.checked+'}}')); 

//$('#request-rois').addEventListener('click', ev => {
//	getROIs();
//});

//  save ROIs in MAP mode 
$('#saveMAP-rois').addEventListener('click', ev => {
	let payload = "";
	payload = setROIs();
	let payload_head = '{"motion":{"rois":'
	let payload_tail = '}}';
	const json_actions = '"action":{"save_config":null,"restart_thread":8}';
	payload += "["+ "0,0,0,0" +"]]";
	//payload += "["+`${x0}`+","+`${y0}`+","+`${x1}`+","+`${y1}`+"]]";
	sendToEndpoint(payload_head+payload+ "}}");
	sendToEndpoint("{"+ json_actions + "}");
	draw_tiles();
	saveMAPbutton.style.backgroundColor = "gray";
});

var xr = 1;
var yr = 1;
var g_x0 = 0;
var g_y0 = 0;
var g_w0 = 300;
var g_h0 = 200;
var map_h = 8;
var map_v = 6;
var active_tiles = [];
var roi_changed = false;

async function sendToEndpoint(payload) {
	console.log(ts(), '===>', payload);
	try {
		const response = await fetch(endpoint, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: payload
		});
		if (!response.ok) throw new Error('HTTP ${response.status}');
		const contentType = response.headers.get('content-type');
		if (contentType?.includes('application/json')) {
			const msg = await response.json();
			console.log(ts(), '<===', JSON.stringify(msg));
			handleMessage(msg);
		}
	} catch (err) {
		console.error('Send error', err);
	}
}

function saveValue(domain, name) {
	const el = $(`#${domain}_${name}`);
	if (!el) {
		console.error('Element #${domain}_${name} not found');
		return;
	}
	let value;
	if (el.type == "checkbox") {
		value = el.checked ? 'true' : 'false';
	} else {
		value = el.value;
	}
	const json_actions = '"action":{"save_config":null,"restart_thread":8}';
	sendToEndpoint(`{"${domain}":{"${name}":${value}}}`);
	sendToEndpoint("{"+ json_actions + "}");
}

function saveROI(domain) {
    if ($("#motion_mapMode").checked == false) {
	const payload =	'"motion":{' + roi_params.map((x) => `"${x}":` + $(`#${domain}_${x}`).value).join() + '}';
	const json_actions = '"action":{"save_config":null,"restart_thread":8}';
	sendToEndpoint("{" + payload + "}");
	sendToEndpoint("{"+ json_actions + "}");
    }
}


//  either send ROIs from MAP to server or 
function setROIs() {
    if ($("#motion_mapMode").checked == true) {
	let payload = "[";
	let mx0 = 0;
	let my0 = 0;
	let mx1 = 0;
	let my1 = 0;
	let i, j;
	let has_tile = false;
	const dx = canvas.width / map_h;
	const dy = canvas.height / map_v;
	for (j = 0; j < map_v; j++)
	{
		my0 = dy * j;
		my1 = dy * (j+1);
		for (i = 0; i < map_h; i++) 
		{
			if (active_tiles[i + (j * map_h)] == true) {
			has_tile = true;
			mx0 = dx * i;
			mx1 = dx * (i+1);
    			payload += "["+`${mx0}`+","+`${my0}`+","+`${mx1}`+","+`${my1}`+"]"; 
			payload += ",";
			}
		}
	}
	console.log("ROI array => ",payload);
	return payload;
    }
}

// request ROIs from server
function getROIs() {
	let payload = '{"motion":{"rois":null}}';
	sendToEndpoint(payload);
}

function parse_ROI(rois) {
	let i;
	let x0, y0, x1, y1;
	for (i=0; i < rois.length; i++) {
		x0 = rois[i][0];
		y0 = rois[i][1];
		x1 = rois[i][2];
		y1 = rois[i][3];
    		if ($("#motion_mapMode").checked == true) {
			box2tile(x0,y0,x1,y1);
		}
	}
}

// clear local ROIs MAP
$('#clear_ROI').addEventListener('click', ev => {
	init_tiles(false);
	saveMAPbutton.style.backgroundColor = 'rgba(180, 100, 100, 0.9)';
});

loadConfig().then(() => { 
//	$('#preview').src = '/x/ch1.mjpg' 
});

loadConfig();

// initialize local ROIs in MAP either all set or reset
function init_tiles(en) {
	active_tiles = [];
	let i;
	for (i=0; i < 48; i++) {
		active_tiles.push(en);
	}
    	ttx.lineWidth = 6; 
	ttx.fillStyle = 'rgba(150, 150, 150, 0.4)'; 
	if (en) {
	} else 
	{
		ttx.clearRect(0, 0, canvas.width, canvas.height);
	}
}

//  draw MAP active tiles to tileOnImage plane 
function draw_tiles() {
	let i, j;
	const dx = canvas.width / map_h;
	const dy = canvas.height / map_v;
    	ttx.lineWidth = 4; // Border thickness
	ttx.fillStyle = 'rgba(210, 200, 200, 0.4)'; 
	let xsc = 0;
	let ysc = 0;
	ttx.clearRect(0, 0, canvas.width, canvas.height);
	for (j=0; j < map_v; j++) {
		ysc = j * dy;
		for (i=0; i < map_h; i++) {
			if (active_tiles[i+j*map_h] == true) {
				xsc = i * dx;
				ttx.fillRect(xsc, ysc, dx, dy);
			}
		}
	}
}

//  Plane for tiles
var tiles = document.getElementById('tileOnImage');
var ttx = tiles.getContext('2d');
//  plane for grid 
var grid = document.getElementById('gridOnImage');
var gtx = grid.getContext('2d');
//  plane for draw box
var canvas = document.getElementById('imageCanvas');
var ctx = canvas.getContext('2d');
//  inner image
var img = document.getElementById('preview');
let saveMAPbutton = document.getElementById("saveMAP-rois");

img.addEventListener('load', 
    function get_current () {
	//  load once
	img.removeEventListener('load', get_current, false);
	canvas.width = img.naturalWidth;
	canvas.height = img.naturalHeight;
	canvas.offsetTop= img.offsetTop;
	tileOnImage.width = img.naturalWidth;
	tileOnImage.height = img.naturalHeight;
	gridOnImage.width = img.naturalWidth;
	gridOnImage.height = img.naturalHeight;
 	xr = canvas.width / canvas.offsetWidth;
 	yr = canvas.height / canvas.offsetHeight;
	//  get box ROI from server and draw to canvas plane
	loadROI();
    	//ctx.strokeStyle = 'rgba(0,255,0, 0.4)'; // green, transparent
	//drawROIbox();
	//  draw grid
	drawGrid(map_h, map_v);
	//  get ROIs from server
	getROIs();
}, false);

$('#preview').src = '/x/ch1.mjpg' 

//  grids
function drawGrid(h, v)
{
	const dx = grid.width / h;
	const dy = grid.height / v;
	let x0, y0;
   	gtx.lineWidth = 2; 
    	gtx.strokeStyle = 'rgba(0, 0, 255, 0.4)'; // blue, transparent
	for (j = 0; j < v; j++)
	{
		y0 = dy * j;
		for (i = 0; i < h; i++) 
		{
			x0 = dx * i;
    			gtx.strokeRect(x0, y0, dx, dy); // (x, y, width, height)
		}
	}
}

//  draw ROI box
function drawROIbox() {
	x0 = (motion_roi_0_x.value);
	y0 = (motion_roi_0_y.value);
	w0 = (motion_roi_1_x.value - motion_roi_0_x.value) + 1;
	h0 = (motion_roi_1_y.value - motion_roi_0_y.value) + 1;
   	ctx.lineWidth = 4; // Border thickness
    	ctx.strokeRect(x0, y0, w0, h0); // (x, y, width, height)
	g_x0 = x0;
	g_y0 = y0;
	g_w0 = w0;
	g_h0 = h0;
}

//  convert box to tiles
function box2tile(x0,y0,x1,y1) {
	const dx = tileOnImage.width / map_h;
	const dy = tileOnImage.height / map_v;
    	ttx.lineWidth = 6; 
	ttx.fillStyle = 'rgba(180, 150, 150, 0.4)'; 
	let xsc, ysc, xec, yec;
	for (j=0; j < map_v; j++) {
		if ( ((j+1)*dy > y0) && ((j*dy) < y1)) {
			for (i=0; i < map_h; i++) {
				if ( ((i+1)*dx > x0) && ((i*dx) < x1)) {
					xsc = dx * i;
					ysc = dy * j;
					xec = xsc + dx;
					yec = ysc + dy;
					ttx.fillRect(xsc, ysc, dx, dy);
					active_tiles[i+j*map_h] = true;	
		 		}
			}
		}
	}		
}


//loadROI().then(() => {
//});


window.onload = 
    function() {
	var box = document.getElementById("boxOnImage");
        function isInImage(x, y) {
        	var innerImage = document.getElementById("imageCanvas");

                if (innerImage.offsetHeight > y || innerImage.offsetWidth > x) {
                        return true;
                } else {
                        return false;
                }

        }

        var top = 0;
        var left = 0;
        var width = 0;
        var height = 0;
	let xs = -1;
	let ys = -1;
	let xe = -1;
	let ye = -1;
	var moveEnable = false;
	var p_function = 0;  // previous mouse function
	// mouse functions
        imageCanvas.addEventListener("mousemove", function (e) {
		if (!moveEnable) return;
	    	p_function = 3;
                var x = e.offsetX;
                var y = e.offsetY;
                if (isInImage(x, y)) {
                     if ((xe == -1 && ye == -1) && (xs != -1 && ys != -1)) {
                            createRectangleOnImage(xs, x, ys, y);
                     }
                }
        });
	//
        imageCanvas.addEventListener("mousedown", function (e) {
	    	p_function = 1;
                var x = e.offsetX;
                var y = e.offsetY;
                if (isInImage(x, y)) {
    			ctx.lineWidth = 6; // Border thickness
			ctx.clearRect(g_x0-2, g_y0-2, g_w0+6, g_h0+6);
			draw_tiles();
                        if (xs == -1 && ys == -1) {
                            xs = x;
                            ys = y;
                        } else if (xe == -1 && ye == -1) {
                            xe = x;
                            ye = y;
                            createRectangleOnImage(xs, xe, ys, ye);
                        } else {
                            xe = x;
                            ye = y;
                            createRectangleOnImage(xs, xe, ys, ye);
                        }
                    }
		moveEnable = true;
		roi_changed = true;
		saveMAPbutton.style.backgroundColor = 'rgba(180, 100, 100, 0.9)';
        });
	//
        imageCanvas.addEventListener("mouseup", function (e) {
		let i, j;
		let xsc, ysc, xec, yec;	
		let x0, y0, x1, y1, w0, h0;

		moveEnable = false;
                 xs = -1;
                 ys = -1;
                 xe = -1;
                 ye = -1;
                removeRectangleOnImage();
		const dx = imageCanvas.width / map_h;
		const dy = imageCanvas.height / map_v;
    		ttx.lineWidth = 4; 
		ttx.fillStyle = 'rgba(180, 150, 150, 0.4)'; 
	    if (p_function == 3) {  // was moving, end of box
		x0 = Math.trunc(left * xr);
		y0 = Math.trunc(top * yr);
		w0 = Math.trunc(width * xr);
		h0 = Math.trunc(height * yr);
		x1 = x0 + w0;
		y1 = y0 + h0;
		ttx.strokeStyle = 'rgba( 255, 255, 0, 0.4)'; 
		ttx.strokeRect(x0, y0, w0, h0);
		motion_roi_0_x.value= Math.trunc(left * xr);
		motion_roi_0_y.value= Math.trunc(top * yr);
		motion_roi_1_x.value= Math.trunc((left+width) * xr) - 1;
		motion_roi_1_y.value= Math.trunc((top+height) * yr) - 1;
    		if ($("#motion_mapMode").checked == true) {
			box2tile(x0,y0,x1,y1);
		}
		else {
			saveROI("motion");
    			ctx.strokeStyle = 'rgba(255, 255, 0, 0.4)'; 
			drawROIbox();
		}
		g_x0 = x0;
		g_y0 = y0;
		g_w0 = w0;
		g_h0 = h0;
	   } else 
	   {	// was not moving, is a click on a single box
    		if ($("#motion_mapMode").checked == true) {
                	var x = e.offsetX;
                	var y = e.offsetY;
                	if (isInImage(x, y)) {
				//const dx = canvas.width / map_h;
				//const dy = canvas.height / map_v;
				w0 = Math.trunc(x*xr/dx);
				h0 = Math.trunc(y*yr/dy);
				x0 = dx * w0;
				y0 = dy * h0;
				x1 = x1 + dx;
				y1 = y1 + dy;

				if (active_tiles[w0 + (h0*map_h)] == true) {
					ttx.clearRect(x0, y0, dx, dy);
					active_tiles[w0 + (h0*map_h)] = false;
					drawROIbox();
				} else {
					ttx.fillRect(x0, y0, dx, dy);
					active_tiles[w0 + (h0*map_h)] = true;
				}
			}
			g_x0 = x0;
			g_y0 = y0;
			g_w0 = w0;
			g_h0 = h0;
		}
	    }
	    p_function = 2;
        });
	//  double click the mouse, set for full box/MAP
        imageCanvas.addEventListener("dblclick", function (e) {
                 xs = -1;
                 ys = -1;
                 xe = -1;
                 ye = -1;
		let x0, y0, w0, h0;
                 removeRectangleOnImage();
    		ttx.lineWidth = 6; 
		ttx.clearRect(x0-2, y0-2, w0+4, h0+4);
                width = imageCanvas.offsetWidth;
                height = imageCanvas.offsetHeight;
                x0 = 0;
                y0 = 0;
                w0 = tileOnImage.width;
                h0 = tileOnImage.height;
                ttx.lineWidth = 4; 
                ttx.strokeStyle = 'rgba( 255, 255, 0, 0.4)'; 
                ttx.strokeRect(x0, y0, w0, h0);
    		if ($("#motion_mapMode").checked == true) {
			init_tiles(true);
			draw_tiles();
		} else 
		{
                	motion_roi_0_x.value= x0;
                	motion_roi_0_y.value= y0;
                	motion_roi_1_x.value= w0-1;
                	motion_roi_1_y.value= h0-1;
                	saveROI("motion");
    			ctx.strokeStyle = 'rgba(255, 255, 0, 0.4)'; // green, transparent
			drawROIbox();
		}
        });
	//  keyboard function
        document.addEventListener("keyup", function (e) {
                 // <27>: escape key Code
                 if (e.keyCode == 27) {
                        xs = -1;
                        ys = -1;
                        xe = -1;
                        ye = -1;
                        removeRectangleOnImage();
                  }
        });
	// draw mouse move box image 
        function createRectangleOnImage(xs, xe, ys, ye) {
                if (box.style.display != 'block') {
                       box.style.display = 'block';
                }
                top = 0;
                left = 0;
                width = 0;
                height = 0;
		let t;
                    if (ys > ye) {
                        height = (ys - ye);
                        top = ye;
                    } else {
                        height = (ye - ys);
                        top = ys;
                    }
                    if (xs > xe) {
                        width = (xs - xe);
                        left = xe;
                    } else {
                        width = (xe - xs);
                        left = xs;
                    }

                box.style.top = Number(top) + "px";
                box.style.left = Number(left) + "px";
                box.style.width = Number(width) + "px";
                box.style.height = Number(height) + "px";

           }
	// erase mouse move box image
        function removeRectangleOnImage() {
                    box.style.display = "none";
        }
}();

</script>

<div class="alert alert-dark ui-debug d-none">
<h4 class="mb-3">Debug info</h4>
<% ex "grep ^motion_ $CONFIG_FILE" %>
</div>

<%in _footer.cgi %>

