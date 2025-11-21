#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Servo.h>

//MPU6050 and Arduino Tutorial
//https://www.youtube.com/watch?v=wTfSfhjhAU0
//
//Arduino Nano use A4(SDA), A5(SCL)


volatile bool sendHigh;
const int MPU_ADDR = 0x68;
volatile int c;

volatile int16_t x;
volatile int16_t y;
volatile int16_t z;

volatile int16_t gyro_x, gyro_y, gyro_z;
volatile int16_t accel_x, accel_y, accel_z;
volatile int16_t temp;

char  tmp_str[7];

char* convert_int16_to_str(int16_t i)
{
  sprintf(tmp_str, "%6d", i);// Function to convert numerical values into Raw Data
  return tmp_str;
}

void setup()
{
  Serial.begin(9600);//Change this to make data processing faster
  Wire.begin();

  //Wakes up MPU from sleep by writing 0 to the Power management register
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  /*
    Changing Accelerometer and Gyroscope config settings.
    Accelerometer register is directly after Gyro, therefore no need to specify which register to write to since it auto increments
  */
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); //Gyroscope settings register for changing sensitivity
  Wire.write(0x08); //Changes sensitivity of the Gyroscope: look at register sheet
  Wire.write(0x10); //Changes sensitivity of Accelerometer: look at register sheet
  Wire.endTransmission(true);

  /*
    SPI slave side setup
  */
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE1)); // (Speed, dataOrder, dataMode(i.e rising edge vs. falling edge))
  pinMode(MISO, OUTPUT);
  pinMode(MOSI, INPUT);
  pinMode(SCK, INPUT);
  pinMode(SS, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  SPCR |= _BV(SPE) | _BV(SPIE);// Enable SPI in slave mode
  sei();
  sendHigh=true;
  c = 0;
}

/*
  Sends each sensor data 1 Byte at a time which is then combined in the Master Simulation Code
  Uses the iterator 'c' as a conditional to make sure each data is sent (Total of 3, i.e. accel_x,y,z)
  Hoping to optimize and minimize use of nested if-statements
*/
ISR(SPI_STC_vect)
{
  /*
    Master uses SPI.transfer(data), where 'data' is used as a condition. 0x61 is 'a' for 
    accelerometer data, 0x67 is 'g' for gyroscope data, 0x74 is 't' for temperature data.
    This mechanism can change depending on the Pico or Nano/Uno.
  */
  if(SPDR == 0x61)
  {
    x = accel_x;
    y = accel_y;
    z = accel_z;
  }
  else if(SPDR == 0x67)
  {
    x = gyro_x;
    y = gyro_y;
    z = gyro_z;
  }
  else if(SPDR == 0x74)
  {
    x = (temp/340.00+36.65);
    y = (temp/340.00+36.65);
    z = (temp/340.00+36.65);
  }
  else
  {
    x = 0x00;
    y = 0x00;
    z = 0x00;
  }

  /*
    These if statements send the data to the SPDR data register for the Master to receive.
    If the number is greater/less than -32768 to 32767, will overflow, but according to
    MPU6050, these values should not exceed either limit.
  */
  if (c==0)
  {
    if(sendHigh)
    {
      SPDR = (x >> 8) & 0xFF;
      sendHigh = false;
    }
    else
    {
      SPDR = x & 0xFF;  
      sendHigh = true;
      c++;
    }
  }
  else if (c==1)
  {
    if (sendHigh)
    {
      SPDR = (y >> 8) & 0xFF;
      sendHigh = false;
    }
    else
    {
      SPDR = y & 0xFF;  
      sendHigh = true;
      c++;
    }
  }
  else if (c==2)
  {
    if (sendHigh)
    {
      SPDR = (z >> 8) & 0xFF;
      sendHigh = false;
    }
    else
    {
      SPDR = z & 0xFF;  
      sendHigh = true;
      c=0;
    }
  }
}

void loop()
{
  //MPU6050 Register Sheet for specific register allocations
  //https://cdn.sparkfun.com/datasheets/Sensors/Accelerometers/RM-MPU-6000A.pdf

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); //Select starting Acceleration Register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true); //Read next 6 registers

  //Top register is High, bottom register is Low hence the bit shift
  accel_x = Wire.read()<<8 | Wire.read();
  accel_y = Wire.read()<<8 | Wire.read();
  accel_z = Wire.read()<<8 | Wire.read();

  //Test Values:
  //accel_x=4660;
  //accel_y=22136;
  //accel_z=30612;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43); //Select starting Gyroscope Register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true); //Read next 6 registers

  //Top register is High, bottom register is Low hence the bit shift
  gyro_x = Wire.read()<<8 | Wire.read();
  gyro_y = Wire.read()<<8 | Wire.read();
  gyro_z = Wire.read()<<8 | Wire.read();

  //Test values:
  //gyro_x=10072;
  //gyro_y=9029;
  //gyro_z=-26505;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x41); //Select starting Temperature Register
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 2, true); //Read next 2 registers
  temp = Wire.read()<<8 | Wire.read();

  /*
    Print statements that can be removed, only used to test program and verify values between Slave and Master
  */
  Serial.print("aX = "); Serial.print (convert_int16_to_str(accel_x));
  Serial.print(" | aY = "); Serial.print (convert_int16_to_str(accel_y));
  Serial.print(" | aZ = "); Serial.print (convert_int16_to_str(accel_z));

  Serial.print(" | gX = "); Serial.print (convert_int16_to_str(gyro_x));//Print out gyroscope data
  Serial.print(" | gY = "); Serial.print (convert_int16_to_str(gyro_y));
  Serial.print(" | gZ = "); Serial.print (convert_int16_to_str(gyro_z));

  Serial.print(" | tmp = "); Serial.print (temp/340.00+36.65);//Print out Temperatrue data
  Serial.println();
}