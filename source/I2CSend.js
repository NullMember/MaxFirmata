inlets = 2;
outlets = 2;
var output = new Array(512);


function list(r){
	if(inlet == 0){ //Address LSB and MSB
		var LSB = arguments[0] & 127;
		output[2] = LSB;
		var MSB = 0;
		if(arguments[1] == "AR1") MSB = MSB | 64; //bit-6 Auto Restart Function (0 = stop, 1 = Restart)
		if(arguments[0] > 127) MSB = MSB | 32; //bit-5 Address Mode (0 = 7-bit, 1 = 10-bit)
		if(arguments[2] == "RO") MSB = MSB | 8;
		if(arguments[2] == "RC") MSB = MSB | 4; //bits 4-3 read/write, 00 = write, 01 = read once, 10 = read continuously, 11 = stop reading
		if(arguments[2] == "RS") MSB = MSB | 24;
		if(arguments[0] > 127) MSB = MSB | arguments[0] >> 7; // bits 2-0 10-bit address bits
		output[3] = MSB;
	}
	if(inlet == 1){ //commands inlet
		output[0] = 0xF0;
		output[1] = 0x76;
		outlet(1, arguments.length);
		for(var i = 4; i <= 512; i++){ //clear old output array
			output[i] = 0;
		}
			
		for(var i = 2; i < arguments.length; i++){ //read new commands and shift
			output[i * 2] = arguments[i - 2] & 127;
			output[(i * 2) + 1] = arguments[i - 2] >> 7;
		}
	}
	outlet(0, output);
}