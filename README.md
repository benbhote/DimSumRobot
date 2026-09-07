# DimSumRobot
This is a personal project of a miniature robot based on a ESP32-S3 microchip.

The original aim was to have a RC robot (from a webpage) with the possibility to plug it on an Astronomical telescope Hadley (more about it here : https://www.printables.com/model/224383-astronomical-telescope-hadley-an-easy-assembly-hig).
But with the actual focuser on my Hadley, the camera sensor is too far from the mirror (I need to explore a solution based on a Barlow lens to solve this problem).

Nonetheless, the project is still really fun because of all the other dimensions of the robot :
- Movement based on two wheels only (self-balancing)
- Measuring distance with an Ultrasonic radar, and ability to make emergency stop
- Reading of temperature and humidity
- Capturing pictures or streaming while in motion
- Managing the SD card (downloading and deleting pictures)

The 3D printed chassis can be found here : https://www.printables.com/model/1834603-dim-sum-robot

# How does it works :
All the controls and sensor readings are available on HTML page hosted on a web server run by the ESP32-S3.
When you first arrive on the page, you will have the status of each sensor and have the option to choose from four different views :
- Diagnostic : Default view when device is connected to the robot whith all sensors states.
- Observation : The robot remains stationary and you can take picture with the camera.
    - Snapshot : Provides a quick preview without saving the image.
    - HD : Captures a high-definition photo by temporarily changing the camera sensor configuration, saving it to the microSD card, and then restoring the low-resolution configuration.
    - Astro : Works similarly, but uses specific settings to allow more light to be captured.
- Exploration : The robot's movement is controlled via an on-page joystick. Stabilization can be activated to use values from the MPU to keep the robot balanced. A video stream can also be enabled to view the robot's surroundings.
- Gallery : All saved photos are available here for downloading or deleting.

When no devices are connected to the robot (and only one can be connected at a time), the robot enters "ghost" mode : nothing is activated except the WiFi.

# Hardware :
- Seeed Studio XIAO ESP32-S3 Sense
- OV3660 Camera sensor (68°)
- DollaTek TB6612FNG
- MT2608 DC-DC boost
- 2 N20 motors 6V 300 RPM
- ARCELI GY-521 MPU6050
- Grove Ultrasonic Ranger
- Grove Temperature & Humidity Sensor(SHT40)
- Battery LiPo 3.7 500mAh 902030

# Known issues :
- Currently, the self-balancing isn't properly working and the robot will move with its bottom touching the floor.
- If the robot is on for extended period of time (more than 30min I'd say), the warmth emit by the ESP32-S3 is building up inside the chassis and mainly going out through the SHT40 slit and impacting the data read by it.
- Depending on how well the wheels are fixed, the robot will not go straight forward.

# Next Step :
- Making the PID loop working properly
- Adding conditional compilation to adapt the code depending on the components inside the robot

# Changelog :
2026-09-06 : Creative the repository
