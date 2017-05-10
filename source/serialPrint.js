inlets = 1;
outlets = 1;
var output = new Array(14);
var port = 0
var counter = 0;

function list(r){
	counter = 0;
	port = 0;
	for(var j = 0; j < arguments[1].length / 5; j++){
		for (var i = 0; i < 14; i++){ // clear array
			if (output[i] == "<undefined>") break;
			output[i] = "<undefined>";
		}
		output[0] = 0xF0;
		output[1] = 0x60;
		output[2] = 0x20 | arguments[0];
		for (var i = counter + 2; i < 7 + counter; i++){ //split words
			output[((i - counter) * 2) - 1] = arguments[1].charCodeAt(i - 2) & 127;
			output[(i - counter) * 2] = arguments[1].charCodeAt(i - 2) >> 7;
		}
		output[13] = 0xF7;
		outlet(0, output);
		counter = counter + 5;
	}
}