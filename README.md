# MaxFirmata
Firmata Client for Max/MSP  
This external is port of [PDFirmata](https://github.com/NullMember/PDFirmata)

# TODO

- [Implement Scheduler](https://github.com/firmata/protocol/blob/master/scheduler.md)

# Description

MaxFirmata is Firmata Client for Cycling74's Max. MaxFirmata supports all features of Firmata protocol except Scheduler.

## Compatibility

Current MaxFirmata client is compatible with Firmata protocol 2.6.0

# Arduino Instructions:

* Depending on your board you might need to install a driver for the serial port. (see http://arduino.cc/en/Guide/HomePage )
* Connect USB cable to Arduino and your computer.
* Start Arduino software.
* Select your board from Tools -> Board
* Select your Serial connection from Tools -> Serial Port
* Open the Firmata program by going to File -> Examples -> Firmata -> StandardFirmata or StandardFirmataPlus (Recommended)
* Upload the Firmata program to your board by pressing the upload button or File -> Upload
* When the upload is finished, you can close the arduino software.

# MaxFirmata Instructions:

Look at the help file provided in this repo.