// Super-simple wrapper around the `arduino-builder` CLI tool
// setting up the neccesary paths
var path = require('path');

var sketch = process.argv[2] || 'build/blink/blink.ino';
var board = process.argv[3] || 'esp8266com:esp8266:nodemcuv2:CpuFrequency=80,UploadSpeed=115200,FlashSize=4M3M'
var arduinoDir = process.env.ARDUINO || process.env.HOME + "/arduino-1.8.1";

var buildDir = path.join(path.dirname(sketch), 'builder');
var builder = path.join(arduinoDir, 'arduino-builder');

var cmd = builder + ' -compile ' + ' -verbose' +
    " -hardware " + path.join(arduinoDir, 'hardware') +
    ' -tools ' + path.join(arduinoDir, 'tools-builder') +
    ' -tools ' + path.join(arduinoDir, 'hardware', 'tools') +
    ' -libraries ' + './' +
    ' -fqbn ' + board +
    ' ' + sketch;
    
console.log(cmd);
