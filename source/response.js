var buffer = new Array(128);
var msg = 0;
var counter = 0;

function msg_int(v){
	if(v == 0xF0){ // if sysex begin
		msg = 1;
	} // !if sysex begin
	else if (v == 0xF7){ // if sysex end
		buffer[counter] = v;
		msg = 0;
		counter = 0;
		outlet(0, buffer);
		clearBuffer();
	} //!if sysex end
	for (var i = 0; i < 16; i++){ //if analog read
		if (v == (0xE0 + i)){
			msg = 2;
			break;
		}
	} //!if analog read
	if (v == 0xF9){ //if protocol version
		msg = 3;
	} //!if protocol version
	if (msg == 1){ // do sysex
		buffer[counter] = v;
		counter++;
	} //!do sysex
	else if (msg == 2){ //do analog read
		buffer[counter] = v;
		counter++;
		if (counter == 3){
			outlet(0, buffer);
			counter = 0;
			msg = 0;
			clearBuffer();
		}
	} //!do analog read
	else if (msg == 3){ //do protocol version
		buffer[counter] = v;
		counter++;
		if (counter == 3){
			outlet(0, buffer);
			counter = 0;
			msg = 0;
			clearBuffer();
		}
	} //!do protocol version
}

function clearBuffer(){
	for (var i = 0; i < 128; i++){
		if (buffer[i] == "<undefined>") break;
		buffer[i] = "<undefined>";
	}
}