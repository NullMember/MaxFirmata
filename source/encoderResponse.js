var buffer = new Array(2);
var sign = 0;

function list(r){
	for (var i = 0; i < 2; i++){
		if (buffer[i] == "<undefined>") break;
		buffer[i] = "<undefined>";
	}
	for (var i = 0; i < (arguments.length / 5); i++){
		buffer[0] = arguments[i * 5] & 63;
		sign = arguments[i * 5] >> 6;
		buffer[1] = arguments[(i * 5) + 1];
		buffer[1] = buffer[1] + (arguments[(i * 5) + 2] << 7);
		buffer[1] = buffer[1] + (arguments[(i * 5) + 3] << 14);
		buffer[1] = buffer[1] + (arguments[(i * 5) + 4] << 21);
		if ((arguments[i * 5] >> 6) == 1){
			buffer[1] = buffer[1] * -1;
		}
		outlet(0, buffer);
	}
}