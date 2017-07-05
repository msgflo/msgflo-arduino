// Super-simple wrapper around the `arduino-builder` CLI tool
// setting up the neccesary paths
var path = require('path');

var sketch = process.argv[2] || 'build/blink/blink.ino';
var board = process.argv[3] || 'esp8266com:esp8266:nodemcuv2:CpuFrequency=80,UploadSpeed=115200,FlashSize=4M3M'
var arduinoDir = process.env.ARDUINO || process.env.HOME + "/arduino-1.8.1";

var buildDir = path.join(path.dirname(sketch), 'builder');
var builder = path.join(arduinoDir, 'arduino-builder');

var prefs = "";
if (board.indexOf('tivac') != -1) {
  var armGcc = path.join(arduinoDir, 'hardware/tools/arm-none-eabi-gcc/4.8.3-2014q1');
  prefs = ' -prefs runtime.tools.arm-none-eabi-gcc.path=' + armGcc;
}

var cmd = builder + ' -compile ' + ' -verbose' +
    " -hardware " + path.join(arduinoDir, 'hardware') +
    ' -tools ' + path.join(arduinoDir, 'tools-builder') +
    ' -tools ' + path.join(arduinoDir, 'hardware', 'tools') +
    prefs +
    ' -libraries ' + './' +
    ' -fqbn ' + board +
    ' ' + sketch;


console.log(cmd);
