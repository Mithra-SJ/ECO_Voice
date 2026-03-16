I wanted to build an IOT project "ECO Voice - Edge based , context aware, Offline Voice Recognition for Home automation"

I use these components

ESP32-S3 - main controller
INMP441 I2S Microphone - voice recognition
PIR Motion Sensor (HC-SR501)- for sensing motion
LDR (Light Dependent Resistor) module - sensing brightness of room 
2-Channel Relay Module (5V, 10A Opto-isolated) - switches light and fan
LED (Red/Green) - appliance status indication
INA219 Current Sensor (±3.2A) - current sensing
Breadboard 
USB Cable (ESP32 – Micro-USB/Type-C)
Table Fan / AC Load - demo of fan
[12v DC Motor/fan-0.3–0.6 A] 

Working:
    i need the appliance to run completely in offline...
the wake word is "ECO",next it should say me "listening for secret code" and  listen for next 30 sec for the secret code 
if i say the crt code it should say "unlocked" and start listening for commands or else it shuld say me "wrong code" and 
i can try as many times within that 30 seconds, after that the system off again i wanna say wake again and then try the password, 
if i say ECO it should switch on, then i will give command like, Light on, light off , fan on, fan off, status(says me the status of the sensor readings) 
[for all activity i need it to reply back] and if i say ECO lock it should lock, after that i wanna say ECO again to wake it again, 
and again i need to say the secret code to unlock the system

sensor working:
     the motion sensor senser motion around and is no motion detected the light fan both will not on it will ask the user "no motion detected would u still want to continue with ur action" 
if user says yes it will on the appliance what the user told, if they say no it wont switch on, 
    the LDR is to sense brightness, it is used only when we need to on light, if the brightness is high i want it to say, "Its bright alredy u still want to switch on light?, and proceeds nly if user says yes
    similarly current sensor will see the load and if it is high it wont allow fan and asks for users wish the second time

i need step by step proper guidance from you to build this system, give me full error free  embedded c  code to do this
