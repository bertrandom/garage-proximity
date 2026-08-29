# garage parking proximity indicator

It is sometimes difficult for me to gauge how far to pull into my garage without slamming into the workshop table.

My partner said, "Just look at the table and where it is in relation to the dashboard. That's your reference point. Stop when you get to the reference point."

I built a proximity indicator instead. I had looked at a few different off-the-shelf solutions (lasers, tennis balls) but I didn't really like any of them. The design of this was heavily inspired from this [project](https://web.archive.org/web/20230604193514/https://hackaday.io/project/177714-esp32-based-precision-parking-assist).

https://github.com/user-attachments/assets/5e28ec60-62f2-48d7-ac87-da383c170af0

## hardware

* [64x64 pixel HUB75 LED panel](https://www.aliexpress.us/item/2251832744994700.html?spm=a2g0o.order_list.order_list_main.63.22351802vOOhiE&gatewayAdapt=glo2usa)
* [ESP32-Trinity](https://esp32trinity.com/)
* [HC-SR04 Ultrasonic Module Distance Sensor](https://www.amazon.com/dp/B01COSN7O6?ref=ppx_yo2ov_dt_b_fed_asin_title)
* [RJ45 Breakout Boards](https://www.amazon.com/dp/B0B2P9C3DK?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1)
* Cat-6 Cable
* [5V 3A DC Power Supply](https://www.amazon.com/dp/B0BV64MHY6?th=1)
* [3D printed standoffs](https://github.com/bertrandom/garage-proximity/blob/main/resources/HUB75_Standoff.stl)
* [M3 8mm screws](https://www.amazon.com/dp/B07CJ9BRCK?th=1)

## how it works

### ultrasonic distance sensor

<img width="600" height="600" alt="15569-Ultrasonic_Distance_Sensor_-_HC-SR04-01a" src="https://github.com/user-attachments/assets/01d29e5e-8131-4594-9c25-955c1dd1200b" />

The HC-SR04 Ultrasonic Distance Sensor has two circles that look like speakers. Well, one of them is a speaker. It emits a sound forward and if there's something in front of it, it'll bounce off that and come back toward the other circle which is a receiver. It calculates how long this took and reports this back to the ESP32.

On the ESP32, we do some math: (Time Duration * Velocity of Sound (340 m/s)) / 2 = Distance.

We run this in a loop so we're constantly receiving what the distance of the car is.

Then we specify a few thresholds:
* Getting Close
* Window of "Good Parking"
* Too Close

For my garage, the range for these thresholds is:

| Threshold      | Range      |
|  ---  |  ---  |
| Getting Close      | 60-150 cm      |
| Window of "Good Parking"      | 40-60 cm      |
| Too Close      | 1-40 cm |

To calculate these thresholds, I wrote a sketch that just displays the current distance onto the LED panel and carefully drove towards the back of my garage with one hand while dangerously recording a video in the other. Then I reviewed the video and wrote down what I thought would be good ranges.

### LED panel

<img width="1221" height="1280" alt="IMG_5871" src="https://github.com/user-attachments/assets/12f27a96-52a0-4184-84af-27416db43a4b" />

Displaying stuff on the LED panel is handled by the ESP32 Trinity and the [ESP32-HUB75-MatrixPanel-DMA
](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) library, which also leverages the [Adafruit GFX](https://github.com/adafruit/adafruit-gfx-library) library.

During the "Getting Close" threshold, we draw a horizontal meter with rainbow colors from left to right. The amount this meter is filled is the percentage of completeness of the threshold range. When the meter completely fills up, we've passed 60 cm and entered the next threshold, Window of "Good Parking".

During the Window of "Good Parking", we display the "cat with heart eyes" emoji.

<img width="64" height="64" alt="smiling cat face with heart-eyes" src="https://github.com/user-attachments/assets/5a420703-3844-4a51-8d23-38d0750ae5c7" />

If the driver keeps getting closer, eventually they'll hit the "Too Close" threshold and we'll show the "woman gesturing no" emoji.

<img width="64" height="64" alt="woman gesturing no 2" src="https://github.com/user-attachments/assets/7452f98c-2634-4537-932b-4d75aa6e3cbe" />

The emojis were extracted from the Apple Color Emoji font using [Emoji Extractor Plus](https://github.com/akfreas/emoji-extractor-plus) as 64x64 PNGs and then converted to Arduino byte arrays using [image2cpp](https://javl.github.io/image2cpp/). Settings chosen that were different than the defaults were:

* Canvas background color: Black
* Draw mode: Horizontal, 2 bytes per pixel (565)
* ESP32 formatting: ✅
* ASCII preview: ✅

After the distance sensor reports that the distance has been in either the Window of "Good Parking" or Too Close thresholds for more than a minute, it stops displaying anything on the display, because we assume that the driver has finished parking by that point.

### a brief aside about the ethernet cable

When I first built this, I quickly realized that where I wanted to position the ultrasonic distance sensor (low to the ground, under a table) was quite far from where I wanted to mount the LED panel (high up on the wall).

I briefly considered hooking up the sensor to another ESP32 and having the two communicate over ESP-NOW, but ultimately I decided that it would be simpler (and with less latency) to just run 4 pretty long wires from the sensor to the panel.

In a moment of genius or idiocy, I googled ethernet breakout boards and discovered that I could just run 4 female-to-female dupont cables from the sensor to the breakout board, run CAT 6 from the breakout board to another breakout board, and then connect 4 more female-to-female dupont cables back to the ESP32. The Echo and Trig pins on the sensor are eventually hooked up to the SDA and SCL pins on the ESP32 Trinity, and the 5V and GND are hooked up to the red and black screw terminals on the Trinity. The CAT 6 run is pretty tidy, the back of the sensor and the panel are not.

<img width="1920" height="1440" alt="IMG_5869" src="https://github.com/user-attachments/assets/8e440e1e-8818-4667-b175-d5fb8e97cf41" />

The obvious correct thing to do would be to solder header pins onto the SDA and SCL pins on the ESP32 Trinity, but I just stuck a male dupont cable in those and gaffer taped them out of sheer laziness. If it ever falls out I probably will solder them.. or reapply some gaffer tape.

### some weirdness that's worth mentioning

For some reason, a small rectangle on the lower right of my LED panel displays slightly brighter than the rest of the panel. This isn't that noticable but you can see it if you look for it when displaying the cat emoji. I'm not sure if this is because I got a bad LED panel or the power supply isn't supplying enough power but it doesn't really bother me that much.

### wifi

When the device first starts up, the ESP32 connects to WIFI and displays the IP address along with an emoji of a car to the LED panel.

<img width="64" height="64" alt="car" src="https://github.com/user-attachments/assets/e3414c1b-2a01-4734-9313-23fd5eea31b3" />

The device doesn't actually need WIFI, but I've got it mounted pretty high in my garage and it would be a pain to get it down to reprogram it, so connecting to WIFI is solely for being able to update it OTA. If you don't need or want this, you can easily remove it.

It also gives me some flexibility as I have a [ratgdo](https://ratcloud.llc/) hooked up to my garage door, so I could factor in the garage door state (opening, open, closing, closed) but for now I've kept it pretty simple.

### mounting

I've got a few of these LED panels for various projects and I still haven't found a good way to frame or mount them. What I've done here is 3D print some standoffs that I can screw in to the back with M3 screws and then I use 3M Command Strips (Medium) to attach those standoffs to the wall. The whole panel, even with the ESP32 Trinity and ethernet breakout board, is pretty light so I expect it will stay up. The ethernet breakout board is just screwed in to one of the other M3 holes on the LED panel.

### AI disclosure

Almost all of the code is vibe-coded from an LLM or copied from the ESP32 Trinity example code. I've made a few tweaks but if this causes you to drive into the back of your garage, sorry!

### final parting thoughts

* This is my first time using PlatformIO instead of the Arduino IDE and I should probably have made this change sooner for my ESP32 projects. I still find it too heavy for what it needs to be but the library management seems a lot more sane.
* Still impressed with [Tinkercad](https://www.tinkercad.com/) for modeling 3D printed parts - I built the standoffs and a small box for the sensor really quickly using it.

## conclusion

Let me know if you build this or have any thoughts or concerns at bert@bert.org
