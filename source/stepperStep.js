var buffer = new Array(16);

function list(r){
	for (var i = 0; i < 16; i++){
		if (buffer[i] == "<undefined>") break;
		buffer[i] = "<undefined>";
	}
	buffer[0] = 0xF0;
	buffer[1] = 0x72;
	buffer[2] = 0x01;
	buffer[3] = arguments[0];
	buffer[4] = arguments[1];
	buffer[5] = arguments[2] & 127;
	buffer[6] = (arguments[2] >> 7) & 127;
	buffer[7] = (arguments[2] >> 14) & 127;
	buffer[8] = arguments[3] & 127;
	buffer[9] = (arguments[3] >> 7) & 127;
	if (arguments.length > 4){
		buffer[10] = arguments[4] & 127;
		buffer[11] = arguments[4] >> 7;
		buffer[12] = arguments[5] & 127;
		buffer[13] = arguments[5] >> 7;
		buffer[14] = 0xF7;
	}
	else{
		buffer[10] = 0xF7;
	}
	outlet(0, buffer);
}