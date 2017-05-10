inlets = 1;
outlets = 1;
var output = new Array(16);
var baud = 0;
var port = 0;

function list(r){
	for (var i = 0; i < 16; i++){
		if (output[i] == "<undefined>") break;
		output[i] = "<undefined>";
	}
	output[0] = 0xF0; //start sysex
	output[1] = 0x60; //serial
	output[2] = 0x10 | arguments[0]; //serial config | port
	if (arguments[1] < 128){ // 7-bit port
		output[3] = arguments[1];
		baud = 1;
	}
	else if (arguments[1] < 16384){ // 14-bit port
		output[3] = arguments[1] & 127;
		output[4] = arguments[1] >> 7;
		baud = 2;
	}
	else { // > 14-bit port
		output[3] = arguments[1] & 127;
		output[4] = (arguments[1] >> 7) & 127;
		output[5] = (arguments[1] >> 14) & 127;
		baud = 3;
	}
	output[3 + baud] = arguments[2]; //rx pin
	output[4 + baud] = arguments[3]; //tx pin
	output[5 + baud] = 0xF7 //end sysex
	outlet(0, output);
}