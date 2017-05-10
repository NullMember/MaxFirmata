inlets = 1;
outlets = 1;
var output = new Array(3);
var mode = ["INPUT", "OUTPUT", "ANALOG", "PWM", "SERVO", "SHIFT", "I2C",
			"ONEWIRE", "STEPPER", "ENCODER", "SERIAL", "PULLUP"]

function list(r){
	for (var i = 0; i < 3; i++){
		output[i] = 0;
	}
	output[0] = 0xF4;
	output[1] = arguments[0];
	for (var i = 0; i < 12; i++){
		if (arguments[1] == mode[i]){
			output[2] = i;
			outlet(0, output);
			return;
		}
	}
	post("Options:\nINPUT\nOUTPUT\nANALOG\nPWM\nSERVO\nSHIFT\nI2C\nONEWIRE\nSTEPPER\nENCODER\nSERIAL\nPULLUP");
}