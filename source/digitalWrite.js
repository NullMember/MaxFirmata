inlets = 1;
outlets = 1;
var output = new Array(3);

function list(r){
	for (var i = 0; i < 3; i++){
		output[i] = 0;
	}
	output[0] = 0xF5;
	output[1] = arguments[0];
	if (arguments[1] == "LOW" || arguments[1] == 0) output[2] = 0;
	else if (arguments[1] == "HIGH" || arguments[1] == 1) output[2] = 1;
	else{
		post("Only HIGH and LOW allowed");
		return;
	}
	outlet(0, output);
}