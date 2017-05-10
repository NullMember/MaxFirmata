inlets = 1;
outlets = 1;
var buffer = new Array(128);

function list(r){
	for (var i = 0; i < 128; i++){
		if (buffer[i] == "<undefined>") break;
		buffer[i] = "<undefined>";
	}
	for (var i = 0; i < arguments.length; i++){
		buffer[i] = arguments[i * 2];
		buffer[i] = buffer[i] + (arguments[(i * 2) + 1] * 128);
	}
	outlet(0, buffer);
}