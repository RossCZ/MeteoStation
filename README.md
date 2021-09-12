# MeteoStation

This repository contains the main control code for the Meteo Station IoT project based on Wemos D1 Mini (ESP8266) platform. The first meteo station was launched in 7/2017. Since then, it has significantly evolved to the current modular design. The hardware design was unified and the control code was revised for universal usage. Finally, it was fitted into the custom-designed 3D printed case. Two of these meteo stations have been in operation since 12/2019.

The meteo station is capable of measuring temperature on two sensors (inside/outside), atmospheric pressure and relative humidity. Current sensor readings are displayed on the build-in LED screen. Readings taken in 10 minutes intervals are transmitted via Wi-Fi connection to the ThingSpeak servers and can be viewed on a mobile app or downloaded for further analysis. Each meteo station is supplied from a 5V micro USB connector powered directly from the grid or by an external battery.
