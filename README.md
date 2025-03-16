# Raspberry Pi Pico Keyboard

This code is modified from [pico_usb_kbd_mouse](https://github.com/a-h/pico_usb_kbd_mouse) by [Adrian Hesketh](https://github.com/a-h), which is essentially a minimal example for the Raspberry Pi Pico adapted from the TinyUSB examples at https://github.com/hathach/tinyusb/tree/master/examples/host/cdc_msc_hid

Adrian's simplified example really made it easier to cut through the noise of the TinyUSB examples. 

I decided to make this a clone and own project as I fundamentally wish to take this project in a different direction. 

While Adrian's project was good to get me started and I was able to send ASCII over UART, I felt this did not offer much more than what using an FTDI chip would. 

## Connecting a Keyboard

To connect a USB keyboard to the Pico you need to connect a Micro USB OTG adapter to convert the Micro USB socket on the Pico into a full size USB socket. It's the same cable that the Pi Zero uses for keyboard input, e.g. https://uk.pi-supply.com/products/pi-zero-usb-adaptor-usb-otg-host-cable

There's only one USB socket on the Pico, so if you use it as a USB input, then you can't use the USB as a serial output for reading the console, or to power the Pi.

To power the Pi, you can apply power to pin 40 - see pinout at https://www.raspberrypi.org/documentation/rp2040/getting-started/#board-specifications

To read the serial output, instead of using the built-in USB, you can use a USB serial dongle connected to pins 1, 2 and 3 on the Pico. I used https://www.amazon.co.uk/DSD-TECH-adapter-FT232RL-Compatible/dp/B07BBPX8B8/ref=pd_lpo_1?pd_rd_i=B07BBPX8B8&psc=1


### Wiring Diagram

Below shows how to wire up both using the RP2040 as the Keyboard host as well as using 2 picos. I recommend the [Waveshare RP2040 Zero](https://www.waveshare.com/rp2040-zero.htm) as it has USB, so if you have a USB C Keyboard (or a USB-A -> USB-C adapter lying around). This makes the setup so much simpler.  

![wiring diagram](https://github.com/Quantum-264/hid-over-uart-keyboard/blob/main/usb-keyboard_bb.png?raw=true)

> Note: I am using the PicoVision as my CPU, I can leave the power and UART connected when programming the RP2040. I have not tested this direct into a Pico, I don't think it will conflict as unlike an Arduino the USB Boot mode doesn't work over UART. If it does conflict, just unplug V-BUS or put a switch on V-BUS for programming mode. 

## What did I change. 

The fundamental change in this project came from my desire to switch away from ASCII over UART. I decided instead to send a custom payload over UART

### Example 1: Y Key pressed

```
010 1 00011101 011 1
```

Breakdown:

||||
|-|:-|:-|
| 101          |   0b101   |	Start bits (Key Event)                                | 
| 1            |   0b1     |	Press Bit (Expected: 1 for Press, 0 for Release)      |
| 00011101     |   0x1D    |	Keycode (Y Key, Expected)                             | 
| 011          |   0b011   |	End bits (Key Event)                                  |
| 1            |   0b1     |	Press Bit (Expected: 1 for Press, 0 for Release)      |

### Example 2: Modifier changed (Left shift and Left Control Pressed)

The modifier Event message works in a slightly different way.

```
110 0 00000011 010 0
```

||||
|-|:-|:-|
| 110          |   0b101   |	Start bits (Modifier Event)                             | 
| 0            |   0b1     |	(unused - other than comparison)                        |
| 00000011     |   0x03    |	Modifier bitfield (Left shift and Left Control Pressed) | 
| 010          |   0b011   |	End bits (Modifier Event)                               |
| 0            |   0b1     |	(unused - other than comparison)                        |


## Code structure

The `main.c` file is the entrypoint of the program. It starts up TinyUSB with `board_init()` and `tusb_init()`, then runs the TinyUSB tasks with `tuh_task()` and the `blink_led_task()` to blink the LED to show that the program is running.

The code related to handling USB input is in `hid_app.c`, this file is included as a source file in the project within the `CMakeLists.txt` file.

