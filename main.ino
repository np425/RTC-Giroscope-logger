#include <SPI.h>
#include <SD.h>
#include <Arduino.h>
#include <Ds1302.h>
#include <Wire.h>

struct GiroData {
  float AcX;
  float AcY;
  float AcZ;

  float Temp;

  float GyX;
  float GyY;
  float GyZ;
};

/* Configuration */
const int serial_baud = 9600;

const int sd_pin_cs = 9;

const int ds1302_pin_ena = 3;
const int ds1302_pin_clk = 6;
const int ds1302_pin_dat = 7;

const int MPU=0x68; 
const float giro_sensitivity = 250.0 / 32768.0;
const float accel_sensitivity = 2.0 / 32768.0; 

String log_csv_name = "log.csv";
const int log_refresh_ms = 20;

/* Globals */
Ds1302 rtc(ds1302_pin_ena, ds1302_pin_clk, ds1302_pin_dat);
Ds1302::DateTime current_time;

File csv_file;

GiroData init_giro_data;
GiroData cur_giro_data;

#define print_log(file, msg) file.print(msg); Serial.print(msg);
#define println_log(file, msg) file.println(msg); file.flush(); Serial.println(msg);

void setup() {
  Serial.begin(serial_baud);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  if (!SD.begin(sd_pin_cs)) {
    Serial.println("SD reader initialization failed!");
    while (1);
  } else {
    Serial.println("SD reader initialization done.");  
  }

  csv_file = setup_file_in_sd(log_csv_name);

  // initialize the RTC
  rtc.init();

  setup_ds1302_start_time();

  setup_giro();
}

void loop() {
  rtc.getDateTime(&current_time);
  
  get_giro_data(&cur_giro_data);
  calibrate_giro_data(&cur_giro_data);

  log_csv(csv_file, current_time, cur_giro_data);

  delay(log_refresh_ms);
}

void setup_giro() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);  
  Wire.write(0);    
  Wire.endTransmission(true);

  get_giro_data(&init_giro_data);
}

File setup_file_in_sd(String file_name) {
  SD.remove(log_csv_name);
  File file = SD.open(log_csv_name, FILE_WRITE);

  print_csv_headings(file);
  
  return file;
}

void setup_ds1302_start_time() {
  // test if clock is halted an d set a date-time (see example 2) to start it
  if (rtc.isHalted()) {
    Serial.println("RTC is halted. Setting time...");

    current_time = {
        .year = 23,
        .month = Ds1302::MONTH_DEC,
        .day = 12,
        .hour = 9,
        .minute = 34,
        .second = 30,
        .dow = Ds1302::DOW_FRI
    };

    rtc.setDateTime(&current_time);
  }
}

void print_csv_headings(File file) {
  println_log(file, "Time,AcX,AcY,AcZ,Temp,GyX,GyY,GyZ");
}

void log_csv(File file, Ds1302::DateTime datetime, GiroData giro_data) {
  /* Date */
  print_log(file, datetime.year);    // 00-99
  print_log(file, '-');
  print_log(file, datetime.month);   // 01-12
  print_log(file, '-');
  print_log(file, datetime.day);     // 01-31
  print_log(file, ' ');

  // /* Time */
  print_log(file, datetime.hour);    // 00-23
  print_log(file, ':');
  print_log(file, datetime.minute);  // 00-59
  print_log(file, ':');
  print_log(file, datetime.second);  // 00-59
  print_log(file, ',');

  // /* Accelerometer */
  print_log(file, giro_data.AcX);
  print_log(file, ',');
  print_log(file, giro_data.AcY);
  print_log(file, ',');
  print_log(file, giro_data.AcZ);
  print_log(file, ',');

  /* Temperature */
  print_log(file, giro_data.Temp);
  print_log(file, ',');
  
  /* Gyroscope */
  print_log(file, giro_data.GyX);
  print_log(file, ',');
  print_log(file, giro_data.GyY);
  print_log(file, ',');
  print_log(file, giro_data.GyZ);

  println_log(file, "");
}

void get_giro_data(GiroData* data){
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  
  Wire.endTransmission(false);

  Wire.requestFrom(MPU,14,true);  

  data->AcX=Wire.read()<<8|Wire.read();    
  data->AcY=Wire.read()<<8|Wire.read();  
  data->AcZ=Wire.read()<<8|Wire.read();  

  int16_t temp = Wire.read()<<8|Wire.read();

  // Check if the temperature is negative
  if (temp & 0x8000) {
    // Perform two's complement for negative values
    temp = -((~temp) + 1);
  }

  // Convert to degrees Celsius
  data->Temp = temp / 340.0 + 36.53;
  
  data->GyX=Wire.read()<<8|Wire.read();  
  data->GyY=Wire.read()<<8|Wire.read();  
  data->GyZ=Wire.read()<<8|Wire.read();

  // Sensitivity Calculation
  data->GyX = (data->GyX * giro_sensitivity);
  data->GyY = (data->GyY * giro_sensitivity);
  data->GyZ = (data->GyZ * giro_sensitivity);

  data->AcX = (data->AcX * accel_sensitivity);
  data->AcY = (data->AcY * accel_sensitivity);
  data->AcZ = (data->AcZ * accel_sensitivity);
}

void calibrate_giro_data(GiroData* data) {
  data->AcX -= init_giro_data.AcX;
  data->AcY -= init_giro_data.AcY;
  data->AcZ -= init_giro_data.AcZ;

  data->GyX -= init_giro_data.GyX;
  data->GyY -= init_giro_data.GyY;
  data->GyZ -= init_giro_data.GyZ;
}
