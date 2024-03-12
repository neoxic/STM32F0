Active low signal passthrough
=============================

The firmware provides a way to connect to an ESC on pin A4 through pin A13 (SWDIO) by translating active low signal between the two pins. In other words, if you need to tweak settings or update firmware in your ESC after you have assembled your model, and the only hook-up available is the SWD, you can still gain access to the ESC by flashing this stub firmware and connecting to the SWDIO pin.


Installation
------------

```
cmake -B build
cd build
make
make flash-passthru
```
