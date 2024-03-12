LESU Skid Steer Loader
======================

![](/img/lesu1.jpg)

Check out this video: https://www.youtube.com/watch?v=e72fbO8cWV4


Firmware features
-----------------

+ Hydraulic pump control
+ Dual motor control (tracks)
+ Acceleration ramping
+ LED lighting (headlights, tail light, blinkers, reverse)
+ Backup buzzer (active/passive)
+ iBUS servo link with FlySky transmitter
+ iBUS telemetry (voltage, temperature)
+ MCU: STM32F030F4 (https://stm32-base.org/boards/STM32F030F4P6-VCC-GND)


Rationale
---------

The firmware controls response of the pump depending on input on channels 1, 2 and 5. The main goal is to make the operation smooth and fluid comparing to a simple channel mix in the transmitter. The conventional approach does not take into account the fact that there is some travel distance before a valve begins to open at `VALVE_MIN`. In this model, there is no practical need to account for half-opened state because the valves open quickly. A better strategy is to keep the pump off until a valve is open and then linearly increase output. As a bonus, there is no need to create a channel mix on the transmitter for the pump.

![](/img/pump2.jpg)

The track motors are controlled by the firmware in a differential fashion, i.e. channel 3 is forward/reverse and channel 4 is left/right. This makes it possible to use two independent ESCs (or a dual one working in independent mode) for the tracks without having to configure a channel mix on the transmitter. When `DRIVE_PWM` is enabled, a dual VNH5019 motor driver (or similar) can be used instead of ESCs.

The blinkers indicate the direction of turning while the model is moving forward or turning around. Additionally, there are two forced blinking modes (emergency and strobe) that are controlled by a 3-way switch on channel 7.

The backup buzzer beeps when the model is reversing. Both active and passive buzzers are supported.


Pinout
------

| Pin | I/O | Description         |
|-----|-----|---------------------|
| A3  | IN  | iBUS servo          |
| A2  | I/O | iBUS sens / DEBUG   |
| A9  | OUT | Left track          |
| A1  | OUT | Left track FWD (+)  |
| A5  | OUT | Left track REV (+)  |
| A10 | OUT | Right track         |
| F0  | OUT | Right track FWD (+) |
| F1  | OUT | Right track REV (+) |
| A4  | OUT | Pump                |
| A14 | OUT | Light               |
| A7  | OUT | Backup buzzer       |
| A6  | OUT | Left blinker (*)    |
| B1  | OUT | Right blinker (*)   |
| A13 | OUT | Reverse (*)         |
| A0  | AIN | Voltage             |

(+) `DRIVE_PWM` enabled

(*) active low


Channel mapping
---------------

| # | Action           |
|---|------------------|
| 1 | Bucket           |
| 2 | Lift arm         |
| 3 | Forward/reverse  |
| 4 | Left/right       |
| 5 | Tool             |
| 6 | Light on/off     |
| 7 | Blinkers 1/2/off |


Installation
------------

```
cmake -B build
cd build
make
make flash-lesu   # Flash using ST-LINK probe
```

It is also possible to flash the firmware without a probe:

* Set the boot jumper to boot in DFU mode (BOOT0=1).
* Connect a USB-TTL adapter (RX to pin A2, TX to pin A3).
* Press the reset button.
* Run the following command with the correct device name:

```
stm32flash -w lesu.bin /dev/ttyUSB0
```

* Disconnect the USB-TTL adapter.
* Return the jumper to its initial position (BOOT0=0).
* Press the reset button.
