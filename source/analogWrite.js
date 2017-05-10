inlets = 1;
outlets = 1;
var output = new Array(8);
var length = 0;

function list(r){
	for (var i = 0; i < 8; i++){
		if (output[i] == "<undefined>") break;
		output[i] = "<undefined>";
	}
	if (arguments[0] < 16 && arguments[1] < 16383){
		output[0] = 0xE0 + arguments[0];
		output[1] = arguments[1] & 127;
		output[2] = arguments[1] >> 7;
	}
	else{
		output[0] = 0xF0;
		output[1] = 0x6F;
		output[2] = arguments[0];
		if (arguments[1] < 16384){
			output[3] = arguments[1] & 127;
			output[4] = arguments[1] >> 7;
			length = 2;
		}
		else{
			output[3] = arguments[1] & 127;
			output[4] = arguments[1] >> 7;
			output[5] = arguments[1] >> 14;
			output[6] = arguments[1] >> 21;
			length = 4;
		}
		output[3 + length] = 0xF7;
	}
	outlet(0, output);
}