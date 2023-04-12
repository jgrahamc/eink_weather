# eink-weather

An e-ink weather display based on the Inkplate 10 and the Pirate Weather API.

![The display](display.jpg?raw=true "The display in use")

# Build

1. Get an API key from [Pirate Weather](https://pirateweather.net/).

2. Modify params.h with the WiFi network details, latitude and
   longitude of the location you are getting weather for, the API
   key and set the title to be shown on the display.
   
3. Run make. This will build the PNG versions of the SVG icons. If
   you want to change/add icons the originals come from Erik Flowers'
   [Weather Icons](https://erikflowers.github.io/weather-icons/).

   Make will also build the fonts needed by the program. These are
   based on Google's [Roboto](https://fonts.google.com/specimen/Roboto). You
   can change the fonts by changing the ttf files in fonts/ttf and
   then run make. You'll need to update the .ino that references the
   generated .h files.

4. Copy the PNG files onto the SD card to be inserted in the Inkplate.
   There's a make target called "copy" which will do this but you
   may have to modify the Makefile to set the correct destination
   folder.
   
5. Build the .ino with the Arduino IDE and upload it to the Inkplate.


