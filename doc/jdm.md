JDM Caterpillar 963D Loader
===========================

![](/img/jdm1.jpg)


Firmware features
-----------------

+ Hydraulic pump and valve control
+ Dual motor control (tracks)
+ Acceleration ramping
+ LED lighting (headlights, tail light)
+ Temperature-based fan control
+ Sound controller link
+ iBUS servo link with FlySky transmitter
+ iBUS telemetry (voltage, temperature)
+ MCU: STM32F030F4 (https://stm32-base.org/boards/STM32F030F4P6-VCC-GND)


Rationale
---------

The firmware controls response of the pump depending on input on channels 1, 2 and 5. The main goal is to make the operation smooth and fluid comparing to a simple channel mix in the transmitter. The conventional approach does not take into account the fact that there is some travel distance before a valve begins to open at `VALVE_MIN`. In this model, a valve is not immediately fully open upon reaching that point, but rather upon reaching `VALVE_MAX` which takes some more travel. A better strategy is to keep the pump off until a valve is about to start opening and then give the input reduced weight until the valve is fully open. As a bonus, there is no need to create a channel mix on the transmitter for the pump.

![](/img/pump1.jpg)

Another feature is that the digital servos of the valves are also controlled by the firmware. The main reason for that is servo trimming. Due to their design, the valve servos are not centered mechanically in this model hence they must be trimmed. Consequently, the trimming values need to be further taken into account in the firmware for proper pump control. Since there is no point in keeping the same values in two places, they are stored in the firmware only. It simplifies the model's setup on the transmitter to a great extent. As another bonus, the pump/servo refresh rate is fixed at 250Hz.

The track motors are controlled by the firmware in a differential fashion, i.e. channel 3 is forward/reverse and channel 4 is left/right. This makes it possible to use two independent ESCs (or a dual one working in independent mode) for the tracks without having to configure a channel mix on the transmitter.


Pinout
------

| Pin | I/O | Description       |
|-----|-----|-------------------|
| A3  | IN  | iBUS servo        |
| A2  | I/O | iBUS sens / DEBUG |
| A6  | OUT | Bucket valve      |
| A7  | OUT | Lift arm valve    |
| B1  | OUT | Ripper valve      |
| A4  | OUT | Pump              |
| A9  | OUT | Left track        |
| A10 | OUT | Right track       |
| A14 | OUT | Light             |
| A5  | OUT | Fan               |
| F0  | OUT | Sound (control)   |
| F1  | OUT | Sound (throttle)  |
| A0  | AIN | Temperature       |
| A1  | AIN | Voltage           |
| A13 | OUT | Status LED (*)    |

(*) active low


Channel mapping
---------------

| # | Action          |
|---|-----------------|
| 1 | Bucket          |
| 2 | Lift arm        |
| 3 | Forward/reverse |
| 4 | Left/right      |
| 5 | Ripper          |
| 6 | Light on/off    |
| 7 | Sound on/off    |
| 8 | Horn on/off     |


Installation
------------

```
cmake -B build
cd build
make
make flash-jdm   # Flash using ST-LINK probe
```

It is also possible to flash the firmware without a probe:

* Set the boot jumper to boot in DFU mode (BOOT0=1).
* Connect a USB-TTL adapter (RX to pin A2, TX to pin A3).
* Press the reset button.
* Run the following command with the correct device name:

```
stm32flash -w jdm.bin /dev/ttyUSB0
```

* Disconnect the USB-TTL adapter.
* Return the jumper to its initial position (BOOT0=0).
* Press the reset button.
