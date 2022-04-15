# DK-LaserProjector
This is an Arduino based laser light show using high-speed galvonometers and a color RGB laser module. It is fully self contained and has hundreds of randomized colorful beam effects. The focus of this project is as an audience-scanning laser, meaning the midair beam effects are the focus, not a laser image drawn on the wall.

![Multi colored bounce beams](/images/bounce_magenta.jpg) ![Laser hardware](/images/laser_hardware.jpg)

## Goal
My goal was to make a microcontroller driven laser show using commonly available parts sourced from China. The most expensive part is the XY galvonometer and mirror assembly, which comes paired with 2 galvonometer driver boards that accept [ILDA](https://www.laserworld.com/en/laser-technical-faq/1140-how-is-the-ilda-connector-pinout.html) laser input.  So I needed to figure out how an Arduino could output the correct analog voltages for controlling the motion, and output digital TTL signals to switch the 3 laser color diodes on and off.

![A room full of laser](/images/laser_mirror.jpg)

## Results
This turned out to be one of my favorite projects. I haven't seen anyone do something quite like this with Arduino, and the results exceeded my expectations.  I've programmed about 30 beam animations, with multiple rotation, color, and strobe modes for a continually randomized laser show. Everything is adjustable by the onboard LCD screen menu and rotary encoder knob.

![Purple rotating fan](/images/fan_purple.jpg) ![Blue laser mirror bounced](/images/bounce_blue.jpg)
![Split color with hand](/images/split_hand.jpg) ![PWM red green falling effect](/images/pwm_red_green.jpg)

## Modules
There are several hardware modules working together on this project to produce the final output.
### Laser scanning position
- The Arduino [Metro Mini](https://www.adafruit.com/product/2590) sends out a digital SPI signal to the DAC module
- A 12-bit DAC chip (_MCP4822_) generates dual analog signals for the X and Y axis
- A custom built bipolar amplifer circuit scales the output and generates an [ILDA](https://www.laserworld.com/en/show-laser-light-faq/glossary-definitions/79-i/1316-ilda-eng.html) signal
- The signals are processed by the galvonometer drivers to position the X and Y galvonometer-mounted mirrors, drawing up to 20k points per second. This is what produces the final image that is persistent to the naked eye.
### Laser color modulation
- The Arduino [Metro Mini](https://www.adafruit.com/product/2590) also sends out 3 digital TTL (on/off) signals to the laser module
- A RGB laser module receives those signals to turn the 3 laser colors on and off. This can support PWM, but a more expensive laser module would be needed to support analog modulation.

![Onboard OLED LCD Menu](/images/oled_lcd_menu.jpg)

### Control system
- An OLED LCD screen (_SSD1306_) displays the current animation, rotation mode, color mode, and strobe mode
- A rotary encoder and pause button control the current animation settings on the screen

![Laser sky red](/images/laser_sky_red.jpg)

## Warning
This project has high-voltage components and high-output laser light which can be dangerous. Be careful of the following:
- Mains level high-voltage (110V) is tied into the galvonometer power supply - do not touch!
- I'm using a 200mW class 3B RGB laser module - do not look directly into the beam! This power is enough to damage digital camera sensors as well as eyesight.

![Laser sky green](/images/laser_sky_green.jpg)

## Resources
This project began with the [Arduino Laser Show with Real Galvos by DeltaFlo](https://www.instructables.com/id/Arduino-Laser-Show-With-Real-Galvos/) and built upon to include beam effects from a RGB laser module. I've added a 200mW RGB laser, onboard LCD system, metal chassis, and lots of beam animation programming. Note that you'll need a fog machine to see anything.

## Known issues
There is an issue where changing the settings too quickly freezes the laser show. I suspect it is related to an interrupt conflict between the rotary encoder control and the I2C LCD screen.