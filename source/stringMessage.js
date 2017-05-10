inlets = 1;
outlets = 1;
var buffer = new Array(256);

function list(r){
	for (var i = 0; i < 256; i++){
		buffer[i] = "<undefined>";
	}
	for (var i = 0; i < arguments.length; i++){
		buffer[i] = arguments[i * 2];
		buffer[i] = buffer + (arguments[(i * 2) + 1] * 128);
	}
	outlet(0, buffer);
}