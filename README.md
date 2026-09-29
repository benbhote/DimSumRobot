# DimSumRobot
This is a personal project of a miniature robot based on a ESP32-S3 microchip.

The original aim was to have a RC robot (from a webpage) with the possibility to plug it on an astronomical telescope Hadley (more about it here : https://www.printables.com/model/224383-astronomical-telescope-hadley-an-easy-assembly-hig).
But with the actual focuser on my Hadley, the camera sensor is too far from the mirror (I need to explore a solution based on a Barlow lens to solve this problem).

Nonetheless, the project is still really fun because of all the other dimensions of the robot :
- Movement based on two wheels only (self-balancing)
- Measuring distance with an Ultrasonic radar, and ability to make emergency stop
- Reading of temperature and humidity
- Capturing pictures or streaming while in motion
- Managing the SD card (downloading and deleting pictures)
- (NEW!) Making sound by using the N20 motors at very low frequency

The 3D printed chassis can be found here : https://www.printables.com/model/1834603-dim-sum-robot

# How does it works :
All the controls and sensor readings are available on HTML page hosted on a web server run by the ESP32-S3.
When you first arrive on the page, you will have the status of each sensor as well as the battery percentage. You get the option to choose from four different views :
- Diagnostic : Default view when device is connected to the robot whith all sensors states.
- Observation : The robot remains stationary and you can take picture with the camera.
    - Snapshot : Provides a quick preview without saving the image.
    - HD : Captures a high-definition photo by temporarily changing the camera sensor configuration, saving it to the microSD card, and then restoring the low-resolution configuration.
    - Astro : Works similarly, but uses specific settings to allow more light to be captured.
- Exploration : The robot's movement is controlled via an on-page joystick. Stabilization can be activated to use values from the MPU to keep the robot balanced. A video stream can also be enabled to view the robot's surroundings.
- Gallery : All saved photos are available here for downloading or deleting.

When no devices are connected to the robot (and only one can be connected at a time), the robot enters "ghost" mode : nothing is activated except the WiFi.
By using the N20 motors at low frequency, it is possible for the robot to make sound and express itself. For now, the robot will emit a sound when :
- Boot is finish
- Leaving GHOST mode
- Reentering GHOST mode
- Successfully taking a picture
- Battery is low

# Hardware :
- Seeed Studio XIAO ESP32-S3 Sense
- OV3660 Camera sensor (68°)
- DollaTek TB6612FNG
- MT3608 DC-DC boost
- 2 N20 motors 6V 300 RPM
- ARCELI GY-521 MPU6050
- ARCELI ADS1115
- Grove Ultrasonic Ranger
- Grove Temperature & Humidity Sensor(SHT40)
- Battery LiPo Dogcom 1S 3.7 550mAh 150C

You can consult pin mapping of the ESP32-S3 [here](./PINOUT.md).

For the schematic, please consult KiCad folder.

# Known issues :
_Important to note that with the current configuration, no more GPIO pin are available to put another sensor (except an I2C one's that will be added on the bus with the ADS1115, the MPU6050, SHT40 sensors)._

I'm currently using Arduino IDE 2.3.10 with version 3.3.11 of esp32 package by Espressif Systems (the newer 3.3.12 changed something about SRAM/RAM allocation that makes my camera re-init after boot generating a malloc error).

- Microphone available on the sense module isn't activated because its GPIOs are needed elsewhere.
- Currently, the self-balancing isn't properly tuning. So the robot isn't properly achieving balance for now.
- If the robot is on for extended period of time (more than 30min I'd say), the warmth emit by the ESP32-S3 is building up inside the chassis and mainly going out through the SHT40 slit and impacting the data read by it.
- Depending on how well the wheels are fixed, the robot will not go straight forward.

# Next Step :
- Fine tuning of the PID loop to get proper balance
- Finish designing tracks to get another option instead of the wheel
- (Maybe) Adding conditional compilation to adapt the code depending on the components inside the robot

# Changelog :
- 2026-09-28 : Updated the code with battery info on the webview (by using the ADS1115) and using N20 motors to make sound
- 2026-09-14 : Adding KiCad files
- 2026-09-06 : Creating the repository
