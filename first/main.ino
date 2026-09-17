{
  "version": 1,
  "author": "Mikhael",
  "editor": "wokwi",
  "parts": [
    { "type": "wokwi-esp32-devkit-v1", "id": "esp", "top": 0, "left": 0, "attrs": {} },
    { "type": "wokwi-lcd1602", "id": "lcd", "top": -160, "left": 120, "attrs": { "pins": "i2c" } },
    { "type": "wokwi-pushbutton", "id": "btn", "top": 100, "left": 150, "attrs": { "color": "green" } }
  ],
  "connections": [
    [ "esp:GND.1", "lcd:GND", "black", [ "v0" ] ],
    [ "esp:5V", "lcd:VCC", "red", [ "v0" ] ],
    [ "esp:D21", "lcd:SDA", "green", [ "v0" ] ],
    [ "esp:D22", "lcd:SCL", "green", [ "v0" ] ],
    [ "btn:1.L", "esp:D4", "green", [ "v0" ] ],
    [ "btn:2.L", "esp:GND.2", "black", [ "v0" ] ]
  ],
  "dependencies": {}
}