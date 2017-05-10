var buffer = new Array(16);

function list(r){
	for (var i = 0; i < 16; i++){
		if (buffer[i] == "<undefined>") break;
		buffer[i] = "<undefined>";
	}
	buffer[0] = 0xF0;
	buffer[1] = 0x72;
	buffer[2] = 0x00;
	buffer[3] = arguments[0];
	if (arguments[1] == 1) buffer[4] = 0;
	else if (arguments[1] == 2) buffer[4] = 16;
	if (arguments[2] == 1) buffer[4] = buffer[4] | 1;
	else if (arguments[2] == 2) buffer[4] = buffer[4] | 2;
	else if (arguments[2] == 4) buffer[4] = buffer[4] | 4;
	buffer[5] = arguments[3] & 127;
	buffer[6] = arguments[3] >> 7;
	buffer[7] = arguments[4];
	buffer[8] = arguments[5];
	if (arguments[2] > 3){
		buffer[9] = arguments[6];
		buffer[10] = arguments[7];
		buffer[11] = 0xF7;
	}
	else{
		buffer[9] = 0xF7;
	}
	outlet(0, buffer);
}