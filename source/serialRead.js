inlets = 1;
outlets = 1;
var output = new Array(7);
var port = 0;

function list(r){
	for (var i = 0; i < 7; i++){
		output[i] = 0;
	}
	output[0] = 0xF0; //start sysex
	output[1] = 0x60; //serial
	output[2] = 0x30 | arguments[0]; //serial read | port
	if (arguments[1] == "READ" || arguments[1] == 1){ // 7-bit port
		output[3] = 0;
	}
	else if (arguments[1] == "STOP" || arguments[1] == 0){
		output[3] = 1;
	}
	output[4] = arguments[2] & 127;
	output[5] = arguments[2] >> 7;
	output[6] = 0xF7 //end sysex
	outlet(0, output);
}