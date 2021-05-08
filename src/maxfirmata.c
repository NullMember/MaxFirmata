/* 
	PDFirmata: Firmata client for Pute Data
    Copyright (C) 2017-2020  Malik Enes Safak

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "ext.h"
#include "ext_obex.h"

typedef struct _maxfirmata{
    t_object x_obj;
    uint8_t  * buffer;
    uint32_t rawCounter;
    int32_t  rawType; //rawType 0 nothing, 1 sysex
    void * rawOut, * decOut;
}t_maxfirmata;

static t_class * maxfirmata_class;

// function protoypes

void writeBuffer(t_maxfirmata * x, uint8_t * buffer, uint16_t bytec);
uint8_t serialPort(const char * portName);

void maxfirmata_onRawData(t_maxfirmata * x, long f);

void maxfirmata_version(t_maxfirmata * x);
void maxfirmata_firmware(t_maxfirmata * x);
void maxfirmata_capability(t_maxfirmata * x);
void maxfirmata_sampling(t_maxfirmata * x, t_atom_long time);
void maxfirmata_pinMode(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_digitalWrite(t_maxfirmata * x, t_atom_long pin, t_atom_long state);
void maxfirmata_digitalIn(t_maxfirmata * x, t_atom_long port, t_atom_long state);
void maxfirmata_analogMap(t_maxfirmata * x);
void maxfirmata_analogWrite(t_maxfirmata * x, t_atom_long pin, t_atom_long value);
void maxfirmata_analogIn(t_maxfirmata * x, t_atom_long pin, t_atom_long state);
void maxfirmata_pinState(t_maxfirmata * x, t_atom_long pin);
void maxfirmata_serial(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_I2C(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_servo(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_encoder(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_stepper(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_multistepper(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_onewire(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);
void maxfirmata_scheduler(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv);

void decSysex(t_maxfirmata *x);

// !function prototypes

/*  Custom float and 7bit functions. These functions adapted from firmata.js implementation
    firmata.js licensed under MIT https://github.com/firmata/firmata.js
    Copyright (c) 2011-2015 Julian Gautier julian.gautier@alumni.neumont.edu
    Copyright (c) 2015-2019 The Firmata.js Authors (see AUTHORS.md in above link) */

#define MAX_SIGNIFICAND pow(2, 23)

int encodeCustomFloat(float input){
    int sign = input < 0 ? 1 : 0;

    input *= input < 0 ? -1 : 1;
    
    int base10 = floor(log10(input));
    
    // Shift decimal to start of significand
    int exponent = 0 + base10;
    
    input /= pow(10, base10);

    // Shift decimal to the right as far as we can
    while ((input != (int)input) && (input < MAX_SIGNIFICAND)) {
        exponent -= 1;
        input *= 10;
    }

    // Reduce precision if necessary
    while (input > MAX_SIGNIFICAND) {
        exponent += 1;
        input /= 10;
    }

    int result = (int)input;
    exponent += 11;
    result &= 0x7FFFFF;
    result |= (exponent & 0x0F) << 23;
    result |= (sign & 0x01) << 27;

    return result;
}

float decodeCustomFloat(unsigned int input) {
    int exponent = ((input >> 23) & 0x0F) - 11;
    
    int sign = (input >> 27) & 0x01;

    int result = input & 0x7FFFFF;
    result *= sign == 1 ? -1 : 1;

    float resultf = (float)result * pow(10, exponent);

    return resultf;
}

uint8_t * to7bit(uint8_t * src, uint16_t length){
    // we need 8 x 7-bit for every 7 x 8-bit words
    uint16_t i_s = 0, i_d = 0;
    uint16_t destLength = length + (length / 7);
    uint8_t * buffer = (uint8_t *)malloc((destLength + 2) * sizeof(uint8_t));
    buffer[i_d++] = destLength & 0xFF;
    buffer[i_d++] = (destLength >> 8) & 0xFF;
    uint8_t shift = 0;
    uint8_t previous = 0;
    while(i_s < length){
        if(shift == 0){
            buffer[i_d++] = src[i_s] & 0x7F;
            shift++;
            previous = src[i_s] >> 7;
        }
        else{
            buffer[i_d++] = ((src[i_s] << shift) & 0x7f) | previous;
            if(shift == 6){
                buffer[i_d++] = src[i_s] >> 1;
                shift = 0;
            }
            else{
                shift++;
                previous = src[i_s] >> (8 - shift);
            }
        }
        i_s++;
    }

    if(shift > 0){
        buffer[i_d++] = previous;
    }

    return buffer;
}

uint8_t * from7bit(uint8_t * src, uint16_t length){
    uint16_t destLength = length - (length >> 3);
    uint8_t * buffer = (uint8_t *)malloc((destLength + 2) * sizeof(uint8_t));
    buffer[0] = destLength & 0xFF;
    buffer[1] = (destLength >> 8) & 0xFF;
    uint16_t i = 0;
    
    for(i = 0; i < destLength; i++){
        uint16_t j = i << 3;
        uint16_t pos = (j / 7);
        uint16_t shift = j % 7;
        buffer[i + 2] = (src[pos] >> shift) | ((src[pos + 1] << (7 - shift)) & 0xFF);
    }

    return buffer;
}

// !Custom float and 7bit functions

// Constants

const char * pinModes[] = {
    "INPUT",
    "OUTPUT",
    "ANALOG",
    "PWM",
    "SERVO",
    "SHIFT",
    "I2C",
    "ONEWIRE",
    "STEPPER",
    "ENCODER",
    "SERIAL",
    "PULLUP"
};

const uint8_t pinModesLength = sizeof(pinModes) / sizeof(pinModes[0]);

const char * serialPorts[] = {
    "HW0",
    "HW1",
    "HW2",
    "HW3",
    "HW4",
    "HW5",
    "HW6",
    "HW7",
    "SW0",
    "SW1",
    "SW2",
    "SW3",
    "SW4",
    "SW5",
    "SW6",
    "SW7"
};

const uint8_t serialPortsLength = sizeof(serialPorts) / sizeof(serialPorts[0]);

// !Constants

void * maxfirmata_new(){
    t_maxfirmata * x = (t_maxfirmata *)object_alloc(maxfirmata_class);

    uint16_t bufferSize = 1024;

    x->buffer = (uint8_t *)malloc(bufferSize * sizeof(uint8_t));

    intin(x, 1);

	x->rawOut = intout((t_object *) x);
    x->decOut = outlet_new((t_object *) x, NULL);
    
    object_post((t_object *)x, "maxfirmata: Firmata client for Max MSP");
    object_post((t_object *)x, "Copyright (C) 2017-2020  Malik Enes Safak");
    object_post((t_object *)x, "https://github.com/NullMember/MaxFirmata");
    
    return x;
}

void maxfirmata_free(t_maxfirmata * x){
    free(x->buffer);
    return;
}

void ext_main(void * r){
    t_class * c;
    c = class_new("maxfirmata", (method)maxfirmata_new, (method)maxfirmata_free, sizeof(t_maxfirmata), NULL, 0);
    class_addmethod(c, (method)maxfirmata_onRawData, "in1", A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_version, "version", 0);
    class_addmethod(c, (method)maxfirmata_firmware, "firmware", 0);
    class_addmethod(c, (method)maxfirmata_capability, "capability", 0);
    class_addmethod(c, (method)maxfirmata_sampling, "sampling", A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_pinMode, "pinMode", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_digitalWrite, "digitalWrite", A_LONG, A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_digitalIn, "digitalIn", A_LONG, A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_analogMap, "analogMap", 0);
    class_addmethod(c, (method)maxfirmata_analogWrite, "analogWrite", A_LONG, A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_analogIn, "analogIn", A_LONG, A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_pinState, "pinState", A_LONG, 0);
    class_addmethod(c, (method)maxfirmata_serial, "serial", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_I2C, "I2C", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_servo, "servo", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_encoder, "encoder", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_stepper, "stepper", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_multistepper, "multistepper", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_onewire, "onewire", A_GIMME, 0);
    class_addmethod(c, (method)maxfirmata_scheduler, "scheduler", A_GIMME, 0);
    class_register(CLASS_BOX, c);
    maxfirmata_class = c;
}

void writeBuffer(t_maxfirmata * x, uint8_t * buffer, uint16_t bytec){
    uint16_t i = 0;
    while(i < bytec){
        outlet_int(x->rawOut, buffer[i]);
        i++;
    }
}

uint8_t serialPort(const char * portName){
    uint8_t i = 0;
    while(i < serialPortsLength){
        if(strcmp(portName, serialPorts[i]) == 0){
            return i;
        }
    }
    return 255;
}

void maxfirmata_onRawData(t_maxfirmata * x, long f){
    /* SysEx End */
    if(f == 0xF7){
        x->rawType = 0;
        decSysex(x);
    }
    /* All bytes received */
    if(x->rawType == 0){
        x->rawCounter = 0;
    }
    /* Read incoming SysEx bytes */
    else if(x->rawType == 1){
        x->buffer[x->rawCounter] = f;
        x->rawCounter++;
    }
    /* Read and output analog read command */
    else if(x->rawType == 2){
        x->buffer[x->rawCounter] = f;
        x->rawCounter++;
        if(x->rawCounter == 3){
            t_atom buffer[3];
            uint16_t value = x->buffer[1] & 0x7F;
            value |= (x->buffer[2] & 0x7F) << 7;
            atom_setsym(buffer, gensym("analog"));
            atom_setlong(buffer + 1, x->buffer[0]);
            atom_setlong(buffer + 2, value);
            outlet_list(x->decOut, NULL, 3, buffer);
            x->rawType = 0;
        }
    }
    /* Read and output digital read command */
    else if(x->rawType == 3){
        x->buffer[x->rawCounter] = f;
        x->rawCounter++;
        if(x->rawCounter == 3){
            t_atom buffer[10];
            uint16_t value = x->buffer[1] & 0x7F;
            value |= (x->buffer[2] & 0x1) << 7;
            atom_setsym(buffer, gensym("digital"));
            atom_setlong(buffer + 1, x->buffer[0]);
            atom_setlong(buffer + 2, value & 0x01);
            atom_setlong(buffer + 3, (value >> 1) & 0x01);
            atom_setlong(buffer + 4, (value >> 2) & 0x01);
            atom_setlong(buffer + 5, (value >> 3) & 0x01);
            atom_setlong(buffer + 6, (value >> 4) & 0x01);
            atom_setlong(buffer + 7, (value >> 5) & 0x01);
            atom_setlong(buffer + 8, (value >> 6) & 0x01);
            atom_setlong(buffer + 9, (value >> 7) & 0x01);
            outlet_list(x->decOut, NULL, 3, buffer);
            x->rawType = 0;
        }
    }
    /* Major and minor version numbers */
    else if(x->rawType == 4){
        x->buffer[x->rawCounter] = f;
        x->rawCounter++;
        if(x->rawCounter == 3){
            t_atom buffer[3];
            uint8_t major = x->buffer[1] & 0x7F;
            uint8_t minor = x->buffer[2] & 0x7F;
            atom_setsym(buffer, gensym("version"));
            atom_setlong(buffer + 1, major);
            atom_setlong(buffer + 2, minor);
            outlet_list(x->decOut, NULL, 3, buffer);
            x->rawType = 0;
        }
    }
    /* Beginning of SysEx command */
    /* Type 1 */
    if(f == 0xF0){
        x->rawType = 1;
    }
    /* Beginning of analog command */
    /* Type 2 */
    else if(((int)f & 0xF0) == 0xE0){
        x->buffer[x->rawCounter] = (int)f & 0x0F;
        x->rawCounter++;
        x->rawType = 2;
    }
    /* Beginning of digital command */
    /* Type 3 */
    else if(((int)f & 0xF0) == 0x90){
        x->buffer[x->rawCounter] = (int)f & 0x0F;
        x->rawCounter++;
        x->rawType = 3;
    }
    /* Beginning of version number */
    /* Type 4 */
    else if((int)f == 0xF9){
        x->buffer[x->rawCounter] = f;
        x->rawCounter++;
        x->rawType = 4;
    }
}

void maxfirmata_version(t_maxfirmata * x){
    uint8_t buffer = 0xF9;
    writeBuffer(x, &buffer, 1);
}

void maxfirmata_firmware(t_maxfirmata * x){
    uint8_t buffer[3];
    buffer[0] = 0xF0;
    buffer[1] = 0x79;
    buffer[2] = 0xF7;
    writeBuffer(x, buffer, 3);
}

void maxfirmata_capability(t_maxfirmata * x){
    uint8_t buffer[3];
    buffer[0] = 0xF0;
    buffer[1] = 0x6B;
    buffer[2] = 0xF7;
    writeBuffer(x, buffer, 3);
}

void maxfirmata_sampling(t_maxfirmata * x, t_atom_long time){
    int _time = time;
    uint8_t buffer[5];
    buffer[0] = 0xF0;
    buffer[1] = 0x7A;
    buffer[2] = _time & 0x7F;
    buffer[3] = (_time >> 7) & 0x7F;
    buffer[4] = 0xF7;
    writeBuffer(x, buffer, 5);
}

void maxfirmata_pinMode(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 1){
        int _pin = atom_getlong(argv + 0);
        if(_pin < 0 || _pin > 127){
            error("Pin number must between 0-127");
            return;
        }
        uint8_t buffer[3];
        buffer[0] = 0xF4;
        buffer[1] = _pin;
        buffer[2] = 0xFF;
        uint8_t i;
        const char * modeName = atom_getsym(argv + 1)->s_name;
        for(i = 0; i < pinModesLength; i++){
            if(strcmp(modeName, pinModes[i]) == 0){
                buffer[2] = i;
            }
        }
        if(buffer[2] == 0xFF){
            error("Unknown mode");
        }
        else{
            writeBuffer(x, buffer, 3);
        }
    }
}

void maxfirmata_digitalWrite(t_maxfirmata * x, t_atom_long pin, t_atom_long state){
    int _pin = pin;
    int _state = state;
    if(_pin < 0 || _pin > 127){
        error("Pin number must between 0-127");
        return;
    }
    if(_state < 0 || _state > 1){
        error("State must 0 (LOW) or 1 (HIGH)");
        return;
    }
    uint8_t buffer[3];
    buffer[0] = 0xF5;
    buffer[1] = _pin;
    buffer[2] = _state;
    writeBuffer(x, buffer, 3);
}

void maxfirmata_digitalIn(t_maxfirmata * x, t_atom_long port, t_atom_long state){
    int _port = port;
    int _state = state;
    if(_port < 0 || _port > 15){
        error("Port number must between 0-15");
        return;
    }
    uint8_t buffer[2];
    buffer[0] = 0xD0 + _port;
    buffer[1] = _state > 0 ? 1 : 0;
    writeBuffer(x, buffer, 2);
}

void maxfirmata_analogMap(t_maxfirmata * x){
    uint8_t buffer[3];
    buffer[0] = 0xF0;
    buffer[1] = 0x69;
    buffer[2] = 0xF7;
    writeBuffer(x, buffer, 3);
}

void maxfirmata_analogWrite(t_maxfirmata * x, t_atom_long pin, t_atom_long value){
    int _pin = pin;
    int _value = value;
    if(_pin < 0 || _pin > 127){
        error("Pin number must between 0-127");
        return;
    }
    if(_pin < 16 && _value < 0x4000){
        uint8_t buffer[3];
        buffer[0] = 0xE0 + _pin;
        buffer[1] = _value & 0x7F;
        buffer[2] = (_value >> 7) & 0x7F;
        writeBuffer(x, buffer, 3);
    }
    else{
        uint8_t buffer[8];
        buffer[0] = 0xF0;
        buffer[1] = 0x6F;
        buffer[2] = _pin;
        buffer[3] = _value & 127;
        buffer[4] = (_value >> 7) & 127;
        buffer[5] = (_value >> 14) & 127;
        buffer[6] = (_value >> 21) & 127;
        buffer[7] = 0xF7;
        writeBuffer(x, buffer, 8);
    }
}

void maxfirmata_analogIn(t_maxfirmata * x, t_atom_long pin, t_atom_long state){
    int _pin = pin;
    int _state = state;
    if(_pin < 0 || _pin > 15){
        error("Analog pin number must between 0-15");
        return;
    }
    if(_state < 0 || _state > 1){
        error("State must 0 (DISABLE) or 1 (ENABLE)");
        return;
    }
    uint8_t buffer[2];
    buffer[0] = 0xC0 + _pin;
    buffer[1] = _state;
    writeBuffer(x, buffer, 2);
}

void maxfirmata_pinState(t_maxfirmata * x, t_atom_long pin){
    int _pin = pin;
    if(_pin < 0 || _pin > 127){
        error("Pin number must between 0-127");
        return;
    }
    uint8_t buffer[4];
    buffer[0] = 0xF0;
    buffer[1] = 0x6D;
    buffer[2] = _pin;
    buffer[3] = 0xF7;
    writeBuffer(x, buffer, 4);
}

void maxfirmata_serial(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        /* serial config PORT BAUD (RX) (TX) */
        if(strcmp(cmdName, "config") == 0){
            if(argc > 2){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint32_t baud = atom_getlong(argv + 2);
                uint8_t rxPin;
                uint8_t txPin;
                if(argc > 4){
                    rxPin = atom_getlong(argv + 3);
                    if(rxPin > 127){
                        error("Pin number must between 0-127");
                        return;
                    }
                    txPin = atom_getlong(argv + 4);
                    if(txPin > 127){
                        error("Pin number must between 0-127");
                        return;
                    }
                    uint8_t buffer[9];
                    buffer[0] = 0xF0;
                    buffer[1] = 0x60;
                    buffer[2] = 0x10 + port;
                    buffer[3] = baud & 0x7F;
                    buffer[4] = (baud >> 7) & 0x7F;
                    buffer[5] = (baud >> 14) & 0x7F;
                    buffer[6] = rxPin;
                    buffer[7] = txPin;
                    buffer[8] = 0xF7;
                    writeBuffer(x, buffer, 9);
                }
                else{
                    uint8_t buffer[7];
                    buffer[0] = 0xF0;
                    buffer[1] = 0x60;
                    buffer[2] = 0x10 + port;
                    buffer[3] = baud & 0x7F;
                    buffer[4] = (baud >> 7) & 0x7F;
                    buffer[5] = (baud >> 14) & 0x7F;
                    buffer[6] = 0xF7;
                    writeBuffer(x, buffer, 7);
                }
            }
        }
        /* serial print PORT SYMBOL */
        if(strcmp(cmdName, "print") == 0){
            if(argc > 2){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                const char ** stringNames = (const char **)malloc((argc - 2) * sizeof(const char **));
                uint8_t stringLengths = 0;
                uint16_t i = 2;
                while(i < argc){
                    stringNames[i - 2] = atom_getsym(argv + i)->s_name;
                    stringLengths += strlen(stringNames[i - 2]);
                    i++;
                }
                char * stringName = (char *)malloc(stringLengths * sizeof(char));
                strcpy(stringName, stringNames[0]);
                i = 1;
                while(i < argc - 2){
                    strcat(stringName, " ");
                    strcat(stringName, stringNames[i]);
                    i++;
                }
                uint8_t stringLength = strlen(stringName);
                uint8_t * buffer = (uint8_t *)malloc(((stringLength * 2) + 4) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x20 | port;
                for(i = 0; i < stringLength; i++){
                    buffer[3 + (i * 2)] = stringName[i] & 0x7F;
                    buffer[3 + ((i * 2) + 1)] = (stringName[i] >> 7) & 0x7F;
                }
                buffer[(stringLength * 2) + 3] = 0xF7;
                writeBuffer(x, buffer, (stringLength * 2) + 4);
                free(buffer);
                free(stringNames);
                free(stringName);
            }
        }
        /* serial println PORT SYMBOL */
        if(strcmp(cmdName, "println") == 0){
            if(argc > 2){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                const char ** stringNames = (const char **)malloc((argc - 2) * sizeof(const char **));
                uint8_t stringLengths = 0;
                uint16_t i = 2;
                while(i < argc){
                    stringNames[i - 2] = atom_getsym(argv + i)->s_name;
                    stringLengths += strlen(stringNames[i - 2]);
                    i++;
                }
                char * stringName = (char *)malloc(stringLengths * sizeof(char));
                strcpy(stringName, stringNames[0]);
                i = 1;
                while(i < argc - 2){
                    strcat(stringName, " ");
                    strcat(stringName, stringNames[i]);
                    i++;
                }
                uint8_t stringLength = strlen(stringName);
                uint8_t * buffer = (uint8_t *)malloc(((stringLength * 2) + 5) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x20 | port;
                for(i = 0; i < stringLength; i++){
                    buffer[3 + (i * 2)] = stringName[i] & 0x7F;
                    buffer[3 + ((i * 2) + 1)] = (stringName[i] >> 7) & 0x7F;
                }
                buffer[(stringLength * 2) + 3] = '\n';
                buffer[(stringLength * 2) + 4] = 0xF7;
                writeBuffer(x, buffer, (stringLength * 2) + 4);
                free(buffer);
                free(stringNames);
                free(stringName);
            }
        }
        /* serial write PORT ARG0 ARG1 ARG2 ... */
        if(strcmp(cmdName, "write") == 0){
            if(argc > 2){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint8_t * buffer = (uint8_t *)malloc((((argc - 2) * 2) + 4) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x20 | port;
                uint16_t i = 2;
                while(i < argc){
                    buffer[((i - 2) * 2) + 3] = atom_getlong(argv + i) & 0x7F;
                    buffer[((i - 2) * 2) + 4] = (atom_getlong(argv + i) >> 7) & 0x7F;
                    i++;
                }
                buffer[((argc - 2) * 2) + 3] = 0xF7;
                writeBuffer(x, buffer, ((argc - 2) * 2) + 4);
                free(buffer);
            }
        }
        /* serial read PORT READMODE (MAXBYTES) */
        if(strcmp(cmdName, "read") == 0){
            if(argc > 2){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint8_t readMode = atom_getlong(argv + 2);
                if(readMode > 1){
                    error("Read mode must 0 (Continuous) or 1 (Stop after read)");
                    return;
                }
                if(argc > 3){
                    uint32_t maxBytes = atom_getlong(argv + 3);
                    uint8_t buffer[7];
                    buffer[0] = 0xF0;
                    buffer[1] = 0x60;
                    buffer[2] = 0x30 | port;
                    buffer[3] = readMode;
                    buffer[4] = maxBytes & 0x7F;
                    buffer[5] = (maxBytes >> 7) & 0x7F;
                    buffer[6] = 0xF7;
                    writeBuffer(x, buffer, 7);
                }
                else{
                    uint8_t buffer[5];
                    buffer[0] = 0xF0;
                    buffer[1] = 0x60;
                    buffer[2] = 0x30 | port;
                    buffer[3] = readMode;
                    buffer[4] = 0xF7;
                    writeBuffer(x, buffer, 5);
                }
            }
        }
        /* serial close PORT */
        if(strcmp(cmdName, "close") == 0){
            if(argc > 1){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint8_t buffer[4];
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x50 | port;
                buffer[3] = 0xF7;
                writeBuffer(x, buffer, 4);
            }
        }
        /* serial flush PORT */
        if(strcmp(cmdName, "flush") == 0){
            if(argc > 1){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint8_t buffer[4];
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x60 | port;
                buffer[3] = 0xF7;
                writeBuffer(x, buffer, 4);
            }
        }
        /* serial listen PORT */
        if(strcmp(cmdName, "listen") == 0){
            if(argc > 1){
                uint8_t port = serialPort(atom_getsym(argv + 1)->s_name);
                if(port == 255){
                    error("Unknown serial port name");
                    return;
                }
                uint8_t buffer[4];
                buffer[0] = 0xF0;
                buffer[1] = 0x60;
                buffer[2] = 0x70 | port;
                buffer[3] = 0xF7;
                writeBuffer(x, buffer, 4);
            }
        }
    }
}

void maxfirmata_I2C(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        /* I2C rw ADDRESS AUTORESTART RWMODE ARG0 (ARG1) (ARG2) (...) */
        /* AUTORESTART = 0 : Stop, 1 : Restart */
        /* RWMODE = wr : Write, ro : Read Only, rc : Read Continuously, rw : Read Write */
        if(strcmp(cmdName, "rw") == 0){
            if(argc > 4){
                int addr = atom_getlong(argv + 1);
                int autoRestart = atom_getlong(argv + 2);
                const char * rwMode = atom_getsym(argv + 3)->s_name;
                int rwModeInt = 0;
                if(strcmp(rwMode, "wr") == 0){ rwModeInt = 0; }
                else if(strcmp(rwMode, "ro") == 0){ rwModeInt = 1; }
                else if(strcmp(rwMode, "rc") == 0){ rwModeInt = 2; }
                else if(strcmp(rwMode, "sr") == 0){ rwModeInt = 3; }
                else{ error("Unknown rw mode"); return; }
                int LSB = addr & 0x7F;
                int MSB = 0;
                if(addr > 0x7F){ MSB = (addr >> 7) & 0x7; }
                MSB |= (rwModeInt & 0x3) << 2;
                if(addr > 0x7F) MSB |= 0x1 << 4;
                MSB |= (autoRestart & 0x1) << 5;
                uint8_t * buffer = (uint8_t *)malloc((((argc - 4) * 2) + 5) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x76;
                buffer[2] = LSB & 0x7F;
                buffer[3] = MSB & 0x7F;
                uint16_t i = 4;
                while(i < argc){
                    buffer[((i - 4) * 2) + 4] = atom_getlong(argv + i) & 0x7F;
                    buffer[((i - 4) * 2) + 5] = (atom_getlong(argv + i) >> 7) & 0x7F;
                    i++;
                }
                buffer[((argc - 4) * 2) + 4] = 0xF7;
                writeBuffer(x, buffer, ((argc - 4) * 2) + 5);
                free(buffer);
            }
        }
        /* I2C delay DELAY */
        if(strcmp(cmdName, "delay") == 0){
            if(argc > 1){
                int delay = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x78;
                buffer[2] = delay & 0x7F;
                buffer[3] = (delay >> 7) & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        /* I2C config ARG0 (ARG1) (ARG2) ... */
        if(strcmp(cmdName, "config") == 0){
            if(argc > 1){
                uint8_t * buffer = (uint8_t *)malloc((argc + 3) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x78;
                uint16_t i = 1;
                while(i < argc){
                    buffer[i + 1] = atom_getlong(argv + i) & 0x7F;
                    i++;
                }
                buffer[argc + 2] = 0xF7;
                writeBuffer(x, buffer, argc + 3);
                free(buffer);
            }
        }
    }
}

void maxfirmata_servo(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        /* servo config PIN MINPULSE MAXPULSE */
        if(strcmp(cmdName, "config") == 0){
            if(argc > 3){
                int pin = atom_getlong(argv + 1);
                int minPulse = atom_getlong(argv + 2);
                int maxPulse = atom_getlong(argv + 3);
                uint8_t buffer[8];
                buffer[0] = 0xF0;
                buffer[1] = 0x70;
                buffer[2] = pin & 0x7F;
                buffer[3] = minPulse & 0x7F;
                buffer[4] = (minPulse >> 7) & 0x7F;
                buffer[5] = maxPulse & 0x7F;
                buffer[6] = (maxPulse >> 7) & 0x7F;
                buffer[7] = 0xF7;
                writeBuffer(x, buffer, 8);
            }
        }
        /* servo write PIN VALUE */
        if(strcmp(cmdName, "write") == 0){
            if(argc > 2){
                int pin = atom_getlong(argv + 1);
                int value = atom_getlong(argv + 2);
                if(pin < 16 && value < 0x4000){
                    uint8_t buffer[3];
                    buffer[0] = 0xE0 + pin;
                    buffer[1] = value & 0x7F;
                    buffer[2] = (value >> 7) & 0x7F;
                    writeBuffer(x, buffer, 3);
                }
                else{
                    uint8_t buffer[8];
                    buffer[0] = 0xF0;
                    buffer[1] = 0x6F;
                    buffer[2] = pin;
                    buffer[3] = value & 127;
                    buffer[4] = (value >> 7) & 127;
                    buffer[5] = (value >> 14) & 127;
                    buffer[6] = (value >> 21) & 127;
                    buffer[7] = 0xF7;
                    writeBuffer(x, buffer, 8);
                }
            }
        }
    }
}

void maxfirmata_encoder(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        /* encoder attach ENCODER PINA PINB */
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        if(strcmp(cmdName, "attach") == 0){
            if(argc > 3){
                int encoder = atom_getlong(argv + 1);
                int pinA = atom_getlong(argv + 2);
                int pinB = atom_getlong(argv + 3);
                uint8_t buffer[7];
                buffer[0] = 0xF0;
                buffer[1] = 0x61;
                buffer[2] = 0x00;
                buffer[3] = encoder & 0x7F;
                buffer[4] = pinA & 0x7F;
                buffer[5] = pinB & 0x7F;
                buffer[6] = 0xF7;
                writeBuffer(x, buffer, 7);
            }
        }
        /* encoder read ENCODER */
        if(strcmp(cmdName, "read") == 0){
            if(argc > 1){
                int encoder = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x61;
                buffer[2] = 0x01;
                buffer[3] = encoder & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        /* encoder readAll */
        if(strcmp(cmdName, "readAll") == 0){
            uint8_t buffer[4];
            buffer[0] = 0xF0;
            buffer[1] = 0x61;
            buffer[2] = 0x02;
            buffer[3] = 0xF7;
            writeBuffer(x, buffer, 4);
        }
        /* encoder reset ENCODER */
        if(strcmp(cmdName, "reset") == 0){
            if(argc > 1){
                int encoder = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x61;
                buffer[2] = 0x03;
                buffer[3] = encoder & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        /* encoder report ENABLE */
        if(strcmp(cmdName, "report") == 0){
            if(argc > 1){
                int enable = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x61;
                buffer[2] = 0x04;
                buffer[3] = enable & 0x1;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        /* encoder detach ENCODER */
        if(strcmp(cmdName, "detach") == 0){
            if(argc > 1){
                int encoder = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x61;
                buffer[2] = 0x05;
                buffer[3] = encoder & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
    }
}

void maxfirmata_stepper(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        /* stepper config motor driver step enable pin1 pin2 (pin3) (pin4) (enablePin) (invert) */
        if(strcmp(cmdName, "config") == 0){
            if(argc > 6){
                uint8_t motor = atom_getlong(argv + 1);
                uint8_t driver = atom_getlong(argv + 2);
                uint8_t step = atom_getlong(argv + 3);
                uint8_t enable = atom_getlong(argv + 4);
                /* driver is driver or two pin */
                if((driver == 1) | (driver == 2)){
                    uint8_t pin1 = atom_getlong(argv + 5);
                    uint8_t pin2 = atom_getlong(argv + 6);
                    if(argc > 8){
                        uint8_t enablePin = atom_getlong(argv + 7);
                        uint8_t invert = atom_getlong(argv + 8);
                        uint8_t buffer[10];
                        buffer[0] = 0xF0;
                        buffer[1] = 0x62;
                        buffer[2] = 0x00;
                        buffer[3] = motor & 0x7F;
                        buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                        buffer[5] = pin1 & 0x7F;
                        buffer[6] = pin2 & 0x7F;
                        buffer[7] = enablePin & 0x7F;
                        buffer[8] = invert & 0x7F;
                        buffer[9] = 0xF7;
                        writeBuffer(x, buffer, 10);
                    }
                    else if(argc > 7){
                        uint8_t enablePinorInvert = atom_getlong(argv + 7);
                        uint8_t buffer[9];
                        buffer[0] = 0xF0;
                        buffer[1] = 0x62;
                        buffer[2] = 0x00;
                        buffer[3] = motor & 0x7F;
                        buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                        buffer[5] = pin1 & 0x7F;
                        buffer[6] = pin2 & 0x7F;
                        buffer[7] = enablePinorInvert & 0x7F;
                        buffer[8] = 0xF7;
                        writeBuffer(x, buffer, 9);
                    }
                    else{
                        uint8_t buffer[8];
                        buffer[0] = 0xF0;
                        buffer[1] = 0x62;
                        buffer[2] = 0x00;
                        buffer[3] = motor & 0x7F;
                        buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                        buffer[5] = pin1 & 0x7F;
                        buffer[6] = pin2 & 0x7F;
                        buffer[7] = 0xF7;
                        writeBuffer(x, buffer, 8);
                    }
                }
                /* driver is three pin */
                else if(driver == 3){
                    if(argc > 7){
                        uint8_t pin1 = atom_getlong(argv + 5);
                        uint8_t pin2 = atom_getlong(argv + 6);
                        uint8_t pin3 = atom_getlong(argv + 7);
                        if(argc > 9){
                            uint8_t enablePin = atom_getlong(argv + 8);
                            uint8_t invert = atom_getlong(argv + 9);
                            uint8_t buffer[11];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = enablePin & 0x7F;
                            buffer[9] = invert & 0x7F;
                            buffer[10] = 0xF7;
                            writeBuffer(x, buffer, 11);
                        }
                        else if(argc > 8){
                            uint8_t enablePinorInvert = atom_getlong(argv + 8);
                            uint8_t buffer[10];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = enablePinorInvert & 0x7F;
                            buffer[9] = 0xF7;
                            writeBuffer(x, buffer, 10);
                        }
                        else{
                            uint8_t buffer[9];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = 0xF7;
                            writeBuffer(x, buffer, 9);
                        }
                    }
                }
                else if(driver == 4){
                    if(argc > 8){
                        uint8_t pin1 = atom_getlong(argv + 5);
                        uint8_t pin2 = atom_getlong(argv + 6);
                        uint8_t pin3 = atom_getlong(argv + 7);
                        uint8_t pin4 = atom_getlong(argv + 8);
                        if(argc > 10){
                            uint8_t enablePin = atom_getlong(argv + 9);
                            uint8_t invert = atom_getlong(argv + 10);
                            uint8_t buffer[12];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = pin4 & 0x7F;
                            buffer[9] = enablePin & 0x7F;
                            buffer[10] = invert & 0x7F;
                            buffer[11] = 0xF7;
                            writeBuffer(x, buffer, 12);
                        }
                        else if(argc > 9){
                            uint8_t enablePinorInvert = atom_getlong(argv + 9);
                            uint8_t buffer[11];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = pin4 & 0x7F;
                            buffer[9] = enablePinorInvert & 0x7F;
                            buffer[10] = 0xF7;
                            writeBuffer(x, buffer, 11);
                        }
                        else{
                            uint8_t buffer[10];
                            buffer[0] = 0xF0;
                            buffer[1] = 0x62;
                            buffer[2] = 0x00;
                            buffer[3] = motor & 0x7F;
                            buffer[4] = ((driver & 0x7) << 4) | ((step & 0x7) << 1) | (enable & 0x1);
                            buffer[5] = pin1 & 0x7F;
                            buffer[6] = pin2 & 0x7F;
                            buffer[7] = pin3 & 0x7F;
                            buffer[8] = pin4 & 0x7F;
                            buffer[9] = 0xF7;
                            writeBuffer(x, buffer, 10);
                        }
                    }
                }
                else{
                    error("unknown driver number");
                }
            }
        }
        if(strcmp(cmdName, "zero") == 0){
            if(argc > 1){
                uint8_t motor = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x01;
                buffer[3] = motor & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "step") == 0){
            if(argc > 2){
                uint8_t motor = atom_getlong(argv + 1);
                int32_t step = atom_getlong(argv + 2);
                uint8_t buffer[10];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x02;
                buffer[3] = motor & 0x7F;
                buffer[4] = step & 0x7F;
                buffer[5] = (step >> 7) & 0x7F;
                buffer[6] = (step >> 14) & 0x7F;
                buffer[7] = (step >> 21) & 0x7F;
                buffer[8] = (step >> 28) & 0x1F;
                buffer[9] = 0xF7;
                writeBuffer(x, buffer, 10);
            }
        }
        if(strcmp(cmdName, "to") == 0){
            if(argc > 2){
                uint8_t motor = atom_getlong(argv + 1);
                int32_t position = atom_getlong(argv + 2);
                uint8_t buffer[10];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x03;
                buffer[3] = motor & 0x7F;
                buffer[4] = position & 0x7F;
                buffer[5] = (position >> 7) & 0x7F;
                buffer[6] = (position >> 14) & 0x7F;
                buffer[7] = (position >> 21) & 0x7F;
                buffer[8] = (position >> 28) & 0x1F;
                buffer[9] = 0xF7;
                writeBuffer(x, buffer, 10);
            }
        }
        if(strcmp(cmdName, "enable") == 0){
            if(argc > 2){
                uint8_t motor = atom_getlong(argv + 1);
                int32_t state = atom_getlong(argv + 2);
                uint8_t buffer[6];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x04;
                buffer[3] = motor & 0x7F;
                buffer[4] = state & 0x7F;
                buffer[5] = 0xF7;
                writeBuffer(x, buffer, 6);
            }
        }
        if(strcmp(cmdName, "stop") == 0){
            if(argc > 1){
                uint8_t motor = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x05;
                buffer[3] = motor & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "position") == 0){
            if(argc > 1){
                uint8_t motor = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x06;
                buffer[3] = motor & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "limit") == 0){
            if(argc > 6){
                uint8_t motor = atom_getlong(argv + 1);
                uint8_t lowerPin = atom_getlong(argv + 2);
                uint8_t lowerState = atom_getlong(argv + 3);
                uint8_t upperPin = atom_getlong(argv + 4);
                uint8_t upperState = atom_getlong(argv + 5);
                uint8_t buffer[9];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x07;
                buffer[3] = motor & 0x7F;
                buffer[4] = lowerPin & 0x7F;
                buffer[5] = lowerState & 0x7F;
                buffer[6] = upperPin & 0x7F;
                buffer[7] = upperState & 0x7F;
                buffer[8] = 0xF7;
                writeBuffer(x, buffer, 9);
            }
        }
        if(strcmp(cmdName, "acceleration") == 0){
            if(argc > 2){
                uint8_t motor = atom_getlong(argv + 1);
                float acceleration = atom_getfloat(argv + 2);
                int32_t _acceleration = encodeCustomFloat(acceleration);  
                uint8_t buffer[9];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x08;
                buffer[3] = motor & 0x7F;
                buffer[4] = _acceleration & 0x7F;
                buffer[5] = (_acceleration >> 7) & 0x7F;
                buffer[6] = (_acceleration >> 14) & 0x7F;
                buffer[7] = (_acceleration >> 21) & 0x7F;
                buffer[8] = 0xF7;
                writeBuffer(x, buffer, 9);
            }
        }
        if(strcmp(cmdName, "speed") == 0){
            if(argc > 2){
                uint8_t motor = atom_getlong(argv + 1);
                float speed = atom_getfloat(argv + 2);
                int32_t _speed = encodeCustomFloat(speed);  
                uint8_t buffer[9];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x08;
                buffer[3] = motor & 0x7F;
                buffer[4] = _speed & 0x7F;
                buffer[5] = (_speed >> 7) & 0x7F;
                buffer[6] = (_speed >> 14) & 0x7F;
                buffer[7] = (_speed >> 21) & 0x7F;
                buffer[8] = 0xF7;
                writeBuffer(x, buffer, 9);
            }
        }
    }
}

void maxfirmata_multistepper(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        /* multistep config group motor1 motor2 (motor3) (motor4) ... */
        if(strcmp(cmdName, "config") == 0){
            if(argc > 4){
                uint8_t group = atom_getlong(argv + 1);
                uint8_t * buffer = (uint8_t *)malloc(((argc - 2) + 5) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x20;
                buffer[3] = group & 0x7F;
                uint16_t i = 2;
                while(i < argc){
                    buffer[i + 2] = atom_getlong(argv + i);
                    i++;
                }
                buffer[i + 2] = 0xF7;
                writeBuffer(x, buffer, ((argc - 2) + 5));
                free(buffer);
            }
        }
        /* multistep to group motor1 (motor2) (motor3) (motor4) ... */
        if(strcmp(cmdName, "to") == 0){
            if(argc > 2){
                uint8_t group = atom_getlong(argv + 1);
                uint8_t positionCount = argc - 2;
                int32_t * positions = (int32_t *)malloc(positionCount * sizeof(int32_t));
                uint16_t i = 2;
                while(i < argc){
                    positions[i - 2] = atom_getlong(argv + i);
                    i++;
                }
                uint8_t * buffer = (uint8_t *)malloc(((positionCount * 5) + 5) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x21;
                buffer[3] = group & 0x7F;
                i = 0;
                while(i < positionCount){
                    buffer[((i * 5) + 4)] = positions[i] & 0x7F;
                    buffer[((i * 5) + 4) + 1] = (positions[i] & 0x7F) >> 7;
                    buffer[((i * 5) + 4) + 2] = (positions[i] & 0x7F) >> 14;
                    buffer[((i * 5) + 4) + 3] = (positions[i] & 0x7F) >> 21;
                    buffer[((i * 5) + 4) + 4] = (positions[i] & 0x1F) >> 28;
                    i++;
                }
                buffer[(positionCount * 5) + 4] = 0xF7;
                writeBuffer(x, buffer, ((positionCount * 5) + 5));
                free(buffer);
                free(positions);
            }
        }
        /* multistep stop group */
        if(strcmp(cmdName, "stop") == 0){
            if(argc > 1){
                uint8_t group = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x62;
                buffer[2] = 0x23;
                buffer[3] = group & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
    }
}

void maxfirmata_onewire(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    if(argc > 0){
        const char * cmdName = atom_getsym(argv + 0)->s_name;
        if(strcmp(cmdName, "search") == 0){
            if(argc > 1){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0x40;
                buffer[3] = pin & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "alarmed") == 0){
            if(argc > 1){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0x44;
                buffer[3] = pin & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "config") == 0){
            if(argc > 2){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t power = atom_getlong(argv + 2);
                uint8_t buffer[6];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0x44;
                buffer[3] = pin & 0x7F;
                buffer[4] = power & 0x1;
                buffer[5] = 0xF7;
                writeBuffer(x, buffer, 6);
            }
        }
        if(strcmp(cmdName, "reset") == 0){
            if(argc > 1){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00000001;
                buffer[3] = pin & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "skip") == 0){
            if(argc > 1){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t buffer[5];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00000010;
                buffer[3] = pin & 0x7F;
                buffer[4] = 0xF7;
                writeBuffer(x, buffer, 5);
            }
        }
        if(strcmp(cmdName, "select") == 0){
            if(argc > 2){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t addr[8] = {
                    atom_getlong(argv + 2),
                    atom_getlong(argv + 3),
                    atom_getlong(argv + 4),
                    atom_getlong(argv + 5),
                    atom_getlong(argv + 6),
                    atom_getlong(argv + 7),
                    atom_getlong(argv + 8),
                    atom_getlong(argv + 9)
                };
                uint8_t * address = from7bit(addr, 8);
                uint8_t buffer[14];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00000100;
                buffer[3] = pin & 0x7F;
                buffer[4] = address[0];
                buffer[5] = address[1];
                buffer[6] = address[2];
                buffer[7] = address[3];
                buffer[8] = address[4];
                buffer[9] = address[5];
                buffer[10] = address[6];
                buffer[11] = address[7];
                buffer[12] = address[8];
                buffer[13] = 0xF7;
                writeBuffer(x, buffer, 14);
                free(address);
            }
        }
        /* I assume correlationid is unique id represent which read command is returned with some value
           For now let the user choose this number */
        if(strcmp(cmdName, "read")){
            if(argc > 3){
                uint8_t pin = atom_getlong(argv + 1);
                uint16_t numofbytes = atom_getlong(argv + 2);
                uint16_t correlationid = atom_getlong(argv + 3);
                uint8_t buffer[9];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00001000;
                buffer[3] = pin & 0x7F;
                buffer[4] = numofbytes & 0x7F;
                buffer[5] = (numofbytes >> 7) & 0x7F;
                buffer[6] = correlationid & 0x7F;
                buffer[7] = (correlationid >> 7) & 0x7F;
                buffer[8] = 0xF7;
                writeBuffer(x, buffer, 9);
            }
        }
        if(strcmp(cmdName, "delay") == 0){
            if(argc > 2){
                uint8_t pin = atom_getlong(argv + 1);
                uint32_t delay = atom_getlong(argv + 2);
                uint8_t buffer[9];
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00010000;
                buffer[3] = pin & 0x7F;
                buffer[4] = delay & 0x7F;
                buffer[5] = (delay >> 7) & 0x7F;
                buffer[6] = (delay >> 14) & 0x7F;
                buffer[7] = (delay >> 21) & 0x7F;
                buffer[8] = 0xF7;
                writeBuffer(x, buffer, 9);
            }
        }
        if(strcmp(cmdName, "write") == 0){
            if(argc > 2){
                uint8_t pin = atom_getlong(argv + 1);
                uint8_t * userbytes = (uint8_t *)malloc((argc - 2) * sizeof(uint8_t));
                uint16_t i = 2;
                while(i < argc){
                    userbytes[i - 2] = atom_getlong(argv + i);
                    i++;
                }
                uint8_t * result = to7bit(userbytes, (argc - 2));
                uint16_t resultLength = result[0] | (result[1] << 8);
                uint8_t * buffer = (uint8_t *)malloc((resultLength + 5) * sizeof(uint8_t));
                buffer[0] = 0xF0;
                buffer[1] = 0x73;
                buffer[2] = 0b00100000;
                buffer[3] = pin & 0x7F;
                for(i = 0; i < resultLength; i++){
                    buffer[i + 4] = result[i];
                }
                buffer[resultLength + 4] = 0xF7;
                writeBuffer(x, buffer, resultLength + 5);
                free(buffer);
                free(result);
                free(userbytes);
            }
        }
    }
}

void maxfirmata_scheduler(t_maxfirmata * x, t_symbol * s, long argc, t_atom * argv){
    error("Not yet implemented");
}

void decSysex(t_maxfirmata * x){
    /* Stepper replies */
    if(x->buffer[0] == 0x62){
        /* Stepper position reply */
        if(x->buffer[1] == 0x06){
            uint16_t i = 2;
            uint8_t stepper = x->buffer[i++];
            int32_t position = x->buffer[i++] & 0x7F;
            position |= (x->buffer[i++] & 0x7F) << 7;
            position |= (x->buffer[i++] & 0x7F) << 14;
            position |= (x->buffer[i++] & 0x7F) << 21;
            position |= (x->buffer[i++] & 0x1F) << 28;
            t_atom buffer[4];
            atom_setsym(buffer, gensym("stepper"));
            atom_setsym(buffer + 1, gensym("position"));
            atom_setlong(buffer + 2, stepper);
            atom_setlong(buffer + 3, position);
            outlet_list(x->decOut, NULL, 4, buffer);
        }
        /* Stepper move complete reply */
        if(x->buffer[1] == 0x0A){
            uint16_t i = 2;
            uint8_t stepper = x->buffer[i++];
            int32_t position = x->buffer[i++] & 0x7F;
            position |= (x->buffer[i++] & 0x7F) << 7;
            position |= (x->buffer[i++] & 0x7F) << 14;
            position |= (x->buffer[i++] & 0x7F) << 21;
            position |= (x->buffer[i++] & 0x1F) << 28;
            t_atom buffer[4];
            atom_setsym(buffer, gensym("stepper"));
            atom_setsym(buffer + 1, gensym("complete"));
            atom_setlong(buffer + 2, stepper);
            atom_setlong(buffer + 3, position);
            outlet_list(x->decOut, NULL, 4, buffer);
        }
        /* MultiStepper move complete reply */
        if(x->buffer[1] == 0x24){
            uint8_t group = x->buffer[2];
            t_atom buffer[3];
            atom_setsym(buffer, gensym("multistepper"));
            atom_setsym(buffer + 1, gensym("complete"));
            atom_setlong(buffer + 2, group);
            outlet_list(x->decOut, NULL, 3, buffer);
        }
    }
    /* Firmware name and version reply */
    if(x->buffer[0] == 0x79){
        uint16_t i = 1;
        uint16_t counter = 0;
        uint8_t majorVersion = x->buffer[i++];
        uint8_t minorVersion = x->buffer[i++];
        t_atom buffer[4];
        uint16_t firmwareChar = 0;
        char * firmwareName = (char *)malloc((((x->rawCounter - 3) / 2) + 1) * sizeof(char));
        atom_setsym(buffer, gensym("firmware"));
        atom_setlong(buffer + 1, majorVersion);
        atom_setlong(buffer + 2, minorVersion);
        while(i < x->rawCounter){
            if((i - 3) % 2 == 0){
                firmwareChar = x->buffer[i] & 0x7F;
            }
            else{
                firmwareChar |= (x->buffer[i] & 0x7F) << 7;
                firmwareName[counter] = firmwareChar;
                counter++;
            }
            i++;
        }
        firmwareName[counter] = '\0';
        atom_setsym(buffer + 3, gensym(firmwareName));
        outlet_list(x->decOut, NULL, 4, buffer);
        free(firmwareName);
    }
    /* Firmata string */
    if(x->buffer[0] == 0x71){
        uint16_t i = 1;
        uint16_t counter = 0;
        t_atom buffer[2];
        uint16_t stringChar = 0;
        char * stringName = (char *)malloc((((x->rawCounter - 1) / 2) + 1) * sizeof(char));
        atom_setsym(buffer, gensym("string"));
        while(i < x->rawCounter){
            if((i - 1) % 2 == 0){
                stringChar = x->buffer[i] & 0x7F;
            }
            else{
                stringChar |= (x->buffer[i] & 0x7F) << 7;
                stringName[counter] = stringChar;
                counter++;
            }
            i++;
        }
        stringName[counter] = '\0';
        atom_setsym(buffer + 1, gensym(stringName));
        outlet_list(x->decOut, NULL, 2, buffer);
        free(stringName);
    }
    /* Report encoder(s) position */
    if(x->buffer[0] == 0x61){
        uint8_t encoderCount = (x->rawCounter - 1) / 5; // (counter - first byte (0x61)) / report length
        uint16_t i = 0;
        t_atom buffer[4];
        while(i < encoderCount){
            uint8_t encoder = x->buffer[(i * 5) + 1] & 0x3F;
            uint8_t direction = (x->buffer[(i * 5) + 1] >> 6) & 0x1;
            int32_t position = (int32_t)x->buffer[(i * 5) + 2] & 0x7F;
            position |= ((int32_t)x->buffer[(i * 5) + 3] & 0x7F) << 7;
            position |= ((int32_t)x->buffer[(i * 5) + 4] & 0x7F) << 14;
            position |= ((int32_t)x->buffer[(i * 5) + 5] & 0x7F) << 21;
            position *= direction == 0 ? 1 : -1;
            atom_setsym(buffer, gensym("encoder"));
            atom_setsym(buffer + 1, gensym("reply"));
            atom_setlong(buffer + 2, encoder);
            atom_setlong(buffer + 3, position);
            outlet_list(x->decOut, NULL, 4, buffer);
            i++;
        }
    }
    /* Analog mapping response */
    if(x->buffer[0] == 0x6A){
        uint8_t analog = 0;
        uint16_t i = 1;
        t_atom buffer[3];
        while(i < x->rawCounter){
            analog = x->buffer[i + 1];
            atom_setsym(buffer, gensym("analogMap"));
            atom_setlong(buffer + 1, i - 1);
            atom_setlong(buffer + 2, analog);
            outlet_list(x->decOut, NULL, 3, buffer);
            i++;
        }
    }
    /* Capabilities response */
    if(x->buffer[0] == 0x6C){
        uint16_t i = 1;
        uint8_t mode = 0;
        uint8_t resolution = 0;
        uint8_t pin = 0;
        uint16_t counter = 0;
        t_atom buffer[4];
        atom_setsym(buffer, gensym("capability"));
        atom_setlong(buffer + 1, pin);
        while(i < x->rawCounter){
            if(x->buffer[i] == 0x7F){
                pin++;
                counter = 0;
                atom_setlong(buffer + 1, pin);
            }
            else{
                if(counter % 2 == 0){
                    mode = x->buffer[i];
                    atom_setsym(buffer + 2, gensym(pinModes[mode]));
                }
                else if(counter % 2 == 1){
                    resolution = x->buffer[i];
                    atom_setlong(buffer + 3, resolution);
                    outlet_list(x->decOut, NULL, 4, buffer);
                }
                counter++;
            }
            i++;
        }
    }
    /* Pin state response */
    if(x->buffer[0] == 0x6E){
        uint16_t i = 1;
        uint8_t pin = x->buffer[i++];
        uint8_t mode = x->buffer[i++];
        uint32_t state = 0;
        t_atom buffer[4];
        while(i < x->rawCounter){
            state |= (x->buffer[i] & 0x7F) << (7 * (i - 3));
            i++;
        }
        atom_setsym(buffer, gensym("pinState"));
        atom_setlong(buffer + 1, pin);
        atom_setsym(buffer + 2, gensym(pinModes[mode]));
        atom_setlong(buffer + 3, state);
        outlet_list(x->decOut, NULL, 4, buffer);
    }
    /* Serial read response */
    if(x->buffer[0] == 0x60){
        uint16_t i = 1;
        uint16_t value = 0;
        uint8_t port = x->buffer[i++] & 0x0F;
        t_atom * buffer = (t_atom *)malloc((((x->rawCounter - 2) / 2) + 3) * sizeof(t_atom));
        atom_setsym(buffer, gensym("serial"));
        atom_setsym(buffer + 1, gensym("reply"));
        atom_setsym(buffer + 2, gensym(serialPorts[port]));
        uint16_t counter = 3;
        while(i < x->rawCounter){
            if((i % 2) == 0){
                value = x->buffer[i] & 0x7F;
            }
            else{
                value |= (x->buffer[i] & 0x7F) << 7;
                atom_setlong(buffer + counter, value);
                counter++;
            }
            i++;
        }
        outlet_list(x->decOut, NULL, (((x->rawCounter - 2) / 2) + 2), buffer);
        free(buffer);
    }
    /* serial read response converted to string */
    if(x->buffer[0] == 0x60){
        uint16_t i = 1;
        uint16_t value = 0;
        uint8_t port = x->buffer[i++] & 0x0F;
        t_atom buffer[4];
        char * stringBuffer = (char *)malloc((((x->rawCounter - 2) / 2) + 1) * sizeof(char));
        atom_setsym(buffer, gensym("serial"));
        atom_setsym(buffer + 1, gensym("string"));
        atom_setsym(buffer + 2, gensym(serialPorts[port]));
        uint16_t counter = 0;
        while(i < x->rawCounter){
            if((i % 2) == 0){
                value = x->buffer[i] & 0x7F;
            }
            else{
                value |= (x->buffer[i] & 0x7F) << 7;
                stringBuffer[counter] = value;
                counter++;
            }
            i++;
        }
        stringBuffer[counter] = '\0';
        atom_setsym(buffer + 3, gensym(stringBuffer));
        outlet_list(x->decOut, NULL, 4, buffer);
        free(stringBuffer);
    }
    /* OneWire Reply */
    if(x->buffer[0] == 0x73){
        /* Normal Search Reply */
        if(x->buffer[1] == 0x42){
            uint8_t pin = x->buffer[2];
            uint8_t deviceCount = (x->rawCounter - 3) >> 3;
            t_atom * buffer = (t_atom *)malloc((deviceCount + 3) * sizeof(t_atom));
            atom_setsym(buffer, gensym("onewire"));
            atom_setsym(buffer + 1, gensym("search"));
            atom_setlong(buffer + 2, pin);
            uint8_t i = 0;
            while(i < deviceCount){
                uint64_t address = (uint64_t)x->buffer[(i * 8) + 3];
                address |= ((uint64_t)x->buffer[(i * 8) + 4] & 0x7F) << 7;
                address |= ((uint64_t)x->buffer[(i * 8) + 5] & 0x7F) << 14;
                address |= ((uint64_t)x->buffer[(i * 8) + 6] & 0x7F) << 21;
                address |= ((uint64_t)x->buffer[(i * 8) + 7] & 0x7F) << 28;
                address |= ((uint64_t)x->buffer[(i * 8) + 8] & 0x7F) << 35;
                address |= ((uint64_t)x->buffer[(i * 8) + 9] & 0x7F) << 42;
                address |= ((uint64_t)x->buffer[(i * 8) + 10] & 0x7F) << 49;
                address |= ((uint64_t)x->buffer[(i * 8) + 11] & 0x7F) << 56;
                atom_setlong(buffer + i + 3, address);
                i++;
            }
            outlet_list(x->decOut, NULL, deviceCount + 3, buffer);
            free(buffer);
        }
        /* Alarmed Search Reply */
        if(x->buffer[1] == 0x45){
            uint8_t pin = x->buffer[2];
            uint8_t deviceCount = (x->rawCounter - 3) >> 3;
            t_atom * buffer = (t_atom *)malloc((deviceCount + 3) * sizeof(t_atom));
            atom_setsym(buffer, gensym("onewire"));
            atom_setsym(buffer + 1, gensym("alarmed"));
            atom_setlong(buffer + 2, pin);
            uint8_t i = 0;
            while(i < deviceCount){
                uint64_t address = (uint64_t)x->buffer[(i * 8) + 3];
                address |= ((uint64_t)x->buffer[(i * 8) + 4] & 0x7F) << 7;
                address |= ((uint64_t)x->buffer[(i * 8) + 5] & 0x7F) << 14;
                address |= ((uint64_t)x->buffer[(i * 8) + 6] & 0x7F) << 21;
                address |= ((uint64_t)x->buffer[(i * 8) + 7] & 0x7F) << 28;
                address |= ((uint64_t)x->buffer[(i * 8) + 8] & 0x7F) << 35;
                address |= ((uint64_t)x->buffer[(i * 8) + 9] & 0x7F) << 42;
                address |= ((uint64_t)x->buffer[(i * 8) + 10] & 0x7F) << 49;
                address |= ((uint64_t)x->buffer[(i * 8) + 11] & 0x7F) << 56;
                atom_setlong(buffer + i + 3, address);
                i++;
            }
            outlet_list(x->decOut, NULL, deviceCount + 3, buffer);
            free(buffer);
        }
        /* Onewire Read Reply */
        if(x->buffer[1] == 0x42){
            uint8_t pin = x->buffer[2];
            uint8_t * result = from7bit(&x->buffer[3], x->rawCounter - 3);
            uint16_t resultLength = result[0] | (result[1] << 8);
            t_atom * buffer = (t_atom *)malloc((resultLength + 3) * sizeof(t_atom));
            atom_setsym(buffer, gensym("onewire"));
            atom_setsym(buffer + 1, gensym("reply"));
            atom_setlong(buffer + 2, pin);
            uint16_t i;
            for(i = 0; i < resultLength; i++){
                atom_setlong(buffer + i + 2, result[i]);
            }
            outlet_list(x->decOut, NULL, resultLength + 3, buffer);
            free(buffer);
            free(result);
        }
        
    }
    /* I2C reply */
    if(x->buffer[0] == 0x77){
        uint16_t i = 1;
        uint16_t value = 0;
        uint16_t slaveAddress = x->buffer[i++] & 0x7F;
        slaveAddress |= (x->buffer[i++] & 0x7F) << 7;
        uint16_t registerAddress = x->buffer[i++] & 0x7F;
        registerAddress |= (x->buffer[i++] & 0x7F) << 7;
        t_atom * buffer = (t_atom *)malloc((((x->rawCounter - 5) / 2) + 4) * sizeof(t_atom));
        atom_setsym(buffer, gensym("I2C"));
        atom_setsym(buffer + 1, gensym("reply"));
        atom_setlong(buffer + 2, slaveAddress);
        atom_setlong(buffer + 3, registerAddress);
        uint16_t counter = 4;
        while(i < x->rawCounter){
            if((i - 5) % 2 == 0){
                value = x->buffer[i];
            }
            else{
                value |= (x->buffer[i] & 0x7F) << 7;
                atom_setlong(buffer + counter, value);
                counter++;
            }
            i++;
        }
        outlet_list(x->decOut, NULL, (((x->rawCounter - 5) / 2) + 4), buffer);
        free(buffer);
    }
}
