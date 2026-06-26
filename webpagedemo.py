#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_VL53L0X.h>
#define PWM1 27
#define DIR1 14
#define PWM2 25
#define DIR2 26
#define BNO055_ADDR 0x28
const char* ssid = "Ramakrishnan";
const char* password = "123456789";

WebServer server(80);
Adafruit_VL53L0X lox;
bool sensorAvailable = false;
bool autoMode = false;
String currentDirection = "STOPPED";
String lastDirection = "STOPPED";
int currentPWM = 0;
int lastDir1 = HIGH;
int lastDir2 = HIGH;
int speedValue = 200;
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Robot Controller</title>
<style>
body{font-family:Arial;text-align:center;background:#111;color:white;}
button{width:120px;height:60px;font-size:18px;margin:5px;}
</style>
</head>
<body>
<h1>Robot Controller</h1>
<button onclick="sendCmd('f')">Forward</button><br><br>
<button onclick="sendCmd('l')">Left</button>
<button onclick="sendCmd('s')">Stop</button>
<button onclick="sendCmd('r')">Right</button><br><br>
<button onclick="sendCmd('b')">Backward</button><br><br>
<input type="range" min="0" max="255" value="200" id="spd">
<button onclick="setSpeed()">Speed</button><br><br>
<button onclick="sendCmd('auto')">AUTO</button>
<button onclick="sendCmd('manual')">MANUAL</button>
<div id="telemetry"></div>
<script>
function sendCmd(c){
 fetch('/cmd?c='+encodeURIComponent(c));
}
function setSpeed(){
 let s=document.getElementById('spd').value;
 sendCmd('speed '+s);
}
setInterval(()=>{
 fetch('/telemetry')
 .then(r=>r.text())
 .then(t=>document.getElementById('telemetry').innerHTML=t);
},1000);
</script>
</body>
</html>
)rawliteral";
void writeRegister(uint8_t reg,uint8_t value){
  Wire.beginTransmission(BNO055_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}
int16_t read16(uint8_t reg){
  Wire.beginTransmission(BNO055_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(BNO055_ADDR,2);
  int16_t v=Wire.read();
  v|=(Wire.read()<<8);
  return v;
}
float getYaw(){
  return read16(0x1A)/16.0;
}
int getDistance(){
  if(!sensorAvailable) return -1;
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure,false);
  if(measure.RangeStatus!=4)
    return measure.RangeMilliMeter;
  return -1;
}
void setMotor(int dir1, int dir2, int pwm)
{ // Elvish bhai ke aage koi bol sakta hai kyaaaaa!!
  // check direction change using stored state
  if (dir1 != lastDir1 || dir2 != lastDir2)
  {
    for (int i = currentPWM; i >= 0; i -= 5)
    {
      analogWrite(PWM1, i);
      analogWrite(PWM2, i);
      delay(15);
    }
    currentPWM = 0;
  }

  // update direction
  digitalWrite(DIR1, dir1);
  digitalWrite(DIR2, dir2);

  lastDir1 = dir1;
  lastDir2 = dir2;

  // ramp up smoothly
  for (int i = currentPWM; i <= pwm; i += 5)
  {
    analogWrite(PWM1, i);
    analogWrite(PWM2, i);
    delay(15);
  }

  currentPWM = pwm;
}
void forward(){
  currentDirection="FORWARD";
  setMotor(HIGH,HIGH,speedValue);
}
void backward(){
  currentDirection="BACKWARD";
  setMotor(LOW,LOW,speedValue);
}
void left(){
  currentDirection="LEFT";
  setMotor(LOW,HIGH,speedValue);
}
void right(){
  currentDirection="RIGHT";
  setMotor(HIGH,LOW,speedValue);
}
void stopRobot(){
  currentDirection = "STOPPED";

  // Smooth ramp down
  for(int i = currentPWM; i >= 0; i--){
    analogWrite(PWM1, i);
    analogWrite(PWM2, i);
    delay(10);   // increase for slower stop, decrease for faster stop
  }

  currentPWM = 0;
}
void processCommand(String cmd){
  cmd.trim();
  if(cmd=="f") forward();
  else if(cmd=="b") backward();
  else if(cmd=="l") left();
  else if(cmd=="r") right();
  else if(cmd=="s") stopRobot();
  else if(cmd=="auto") autoMode=true;
  else if(cmd=="manual"){ autoMode=false; stopRobot(); }
  else if(cmd.startsWith("speed ")){
    int s=cmd.substring(6).toInt();
    if(s>=0 && s<=255) speedValue=s;
  }
}
void handleRoot(){
  server.send(200,"text/html",webpage);
}
void handleCmd(){
  processCommand(server.arg("c"));
  server.send(200,"text/plain","OK");
}
void handleTelemetry(){
  String t="";
  t+="Mode: "+String(autoMode?"AUTO":"MANUAL")+"<br>";
  t+="Direction: "+currentDirection+"<br>";
  t+="Speed: "+String(speedValue)+"<br>";
  t+="Distance: "+String(getDistance())+" mm<br>";
  t+="Yaw: "+String(getYaw())+"<br>";
  server.send(200,"text/html",t);
}
void setup(){
  Serial.begin(115200);
  pinMode(DIR1,OUTPUT);
  pinMode(DIR2,OUTPUT);
  pinMode(PWM1,OUTPUT);
  pinMode(PWM2,OUTPUT);
  Wire.begin(21,22);
  if(lox.begin()){
    sensorAvailable=true;
  }
  writeRegister(0x3D,0x00);
  delay(20);
  writeRegister(0x3D,0x0C);
  delay(20);
  stopRobot();
  WiFi.softAP(ssid,password);
  server.on("/",handleRoot);
  server.on("/cmd",handleCmd);
  server.on("/telemetry",handleTelemetry);
  server.begin();
}
void loop(){
  server.handleClient();
  
  if(Serial.available()){
    String cmd=Serial.readStringUntil('\n');
    processCommand(cmd);
  }
  if(autoMode){
    int d=getDistance();
    if(d<0) return;
    if(d>300){
      forward();
    }
    else if(d>150){
      speedValue=120;
      forward();
    }
    else if(d>100){
      stopRobot();
    }
    else{
      backward();
      delay(500);
      right();
      delay(400);
      stopRobot();
    }
  }
}
