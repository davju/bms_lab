/*
 * Basic Software for Battery Management System
 * 
 * These functions deliver the basic funcionality
 * of the BMS.
 * 
 * They must not be changed, but may be analyzed
 * for deeper understanding of the system.
 */
#define TFT_DC 9
#define TFT_CS 10

#define pinTempRX         7
#define pinDriveMode      6 // PWM 980 Hz
#define pinWarnings       5 // PWM 980 Hz
//#define pinOvershoot    4 // ist verbunden mit D6 aber nicht genutzt derzeit
#define pinProdErrors     3 // PWM 490 Hz

#define pinUshunt        A6
#define pinCell1         A0
#define pinCell2         A1
#define pinCell3         A2
#define pinCell4         A3

#define PWMPERIOD      1024 // @ 1ms => 1s

volatile float    Tcell[4], Vcell[4], Vraw[4], Ibat;
volatile int16_t  pwmCell[4] = {-1,-1,-1,-1};
volatile bool     balActive = false; // balancing possibly active but only if needed
volatile uint32_t pwmDuration = 0;								  

volatile byte regWarnings = 0;  // Bit 3: Balancing set in ISR
byte regErrors   = 0;
bool BDUactive   = false;
int  driveMode   = 2; // Random Fast

// Initialize Display object
Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC);

// SOC-Map (Voltate @ Cap => Voltage @ Cell
float SOCx[] = {  0, 0.12, 0.18, 0.3, 0.5, 1.00, 1.10, 2.0};
float SOCy[] = {0.0, 2.50, 3.00, 3.4, 3.7, 4.05, 4.20, 6.9};
// dU/dV             20.8  8.33  3.3  1.5  0.70  1.50  3.0

void fillScreen(uint16_t color)
{
  display.fillScreen(color);                    // fill screen with black
}

void drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  display.drawPixel(x,y,color);  
}

void writeText(int x, int y, int tSize, String txt, uint16_t color)
{
  display.setCursor(x, y);   
  display.setTextSize(tSize);
  display.setTextColor(color,ILI9341_BLACK); // background color black for overwriting
  display.print(txt);
}

void drawRect(int x, int y, int w, int h, uint16_t color)
{
  display.drawRect(x, y, w, h, color);
}

void fillRect(int x, int y, int w, int h, uint16_t color)
{
  display.fillRect(x, y, w, h, color);
}


void drawLine(int x0, int y0, int x1, int y1, uint16_t color)
{
  if (y0 == y1)
    display.drawFastHLine(min(x0,x1), y0, abs(x1-x0), color);
  else  
    display.drawLine(x0,y0,x1,y1,color);
}


uint16_t rgb565(uint16_t r,uint16_t g,uint16_t b)
{
  uint16_t col = (r/8)*0x800+(g/4)*0x0020+(b/8);
  return col;
}

uint16_t colCell(int i)
{
  return rgb565(0,135+i*30,0);
}

uint16_t colTemp(int i)
{
  return rgb565(175+i*20, 113+i*13,   0);
}

String strLen(String strIn, int len)
{
  String strOut = strIn;
  while (strOut.length() < len)
    strOut = " "+strOut;
  return strOut; 
}

float min4(float n1, float n2, float n3, float n4)
{
  return min(min(min(n1,n2),n3),n4);
}

float max4(float n1, float n2, float n3, float n4)
{
  return max(max(max(n1,n2),n3),n4);
}


void setupBSW() 
{
  Serial.begin(9600);
  pinMode(pinTempRX, INPUT);
  pinMode(pinDriveMode, OUTPUT);    digitalWrite(pinDriveMode, HIGH); // off
  pinMode(pinWarnings, OUTPUT);     digitalWrite(pinWarnings, HIGH); // off
  pinMode(pinProdErrors, OUTPUT);   digitalWrite(pinProdErrors, HIGH); // off
  pinMode(pinUshunt, INPUT);
  pinMode(pinCell1, INPUT);
  pinMode(pinCell2, INPUT);
  pinMode(pinCell3, INPUT);
  pinMode(pinCell4, INPUT);
  
  display.begin();                                   // Initialize Display
  display.setRotation(1);                           // rotate screen content
  display.fillScreen(ILI9341_BLACK);                    // fill screen with black
  // HS-Logo
  fillRect(0,0,3,44,ILI9341_WHITE);  
  fillRect(0,0,8,2,ILI9341_WHITE);  
  fillRect(0,44,8,2,ILI9341_WHITE);  
  writeText(12,10,2,"Hochschule Esslingen",ILI9341_RED);
  writeText(41,30,1,"University of Applied Sciences",ILI9341_LIGHTGREY);
  writeText( 30,70,4,"BMS",ILI9341_BLUE);
  writeText(102,70,4,"LAB",ILI9341_RED);
  drawRect(100, 190, 50, 50, ILI9341_WHITE);
  display.fillRect(150, 140, 50, 50, ILI9341_WHITE);

  // Enable PCIE2 and set Mask for PCINT23
  PCICR = 4;
  PCMSK2 = B10000000; // enable only PCINT23 = PD7 = D7
  sei();

  // use TC0 for soft pwm on our analog pins
  // TC0 is used by arduino for functions like delay and millis
  // it is running over roughly once per ms
  // an additional interrupt in the middle is used for us
  OCR0A = 0xA0;
  TIMSK0 |= _BV(OCIE0A);  

  delay(5000); // 5 Seconds minimum

  // wait for VCU to charge up enough if freshly startet
  while (min4(getCellVoltage(1),getCellVoltage(2),getCellVoltage(3),getCellVoltage(4)) < 2.7)
  {
    int i,n;
    setDriveMode(1);
    setBDU_Activation(true); 
    setWarningOvervoltage(false);
    setWarningUndervoltage(false);
    setWarningOvertemp(false);
    drawRect(302,0,18,240,rgb565(150,150,150));   
    for (i = 1; i <= 4; i++)
    {
      n = max(min(((2.7-getCellVoltage(i))/2.7),1),0)*238; 
      fillRect(299+i*4,1,4,n,ILI9341_BLACK);   
      fillRect(299+i*4,n,4,239-n,colCell(i));
    }
    // charge up low cells
    if (getCellVoltage(1) < 2.7) {pinMode(pinCell1, OUTPUT); digitalWrite(pinCell1, HIGH);}
    if (getCellVoltage(2) < 2.7) {pinMode(pinCell2, OUTPUT); digitalWrite(pinCell2, HIGH);}
    if (getCellVoltage(3) < 2.7) {pinMode(pinCell3, OUTPUT); digitalWrite(pinCell3, HIGH);}
    if (getCellVoltage(4) < 2.7) {pinMode(pinCell4, OUTPUT); digitalWrite(pinCell4, HIGH);}
    delay(1000);
    pinMode(pinCell1, INPUT); pinMode(pinCell2, INPUT); pinMode(pinCell3, INPUT); pinMode(pinCell4, INPUT);
    delay(30);
  }
  Serial.println("BMS started");
  fillScreen(ILI9341_BLACK);
}

// Interrupt Service Routine used for Balancing
ISR(TIMER0_COMPA_vect) // called once per ms
{
  static volatile int n0, n1, n2, n3;
  bool pwmActive = false; 
  
  uint32_t dT, tStart = micros();
  /*
  n1 += 950;
  if (n1 >= 1024)
  {
    digitalWrite(2, HIGH);
    n1 -= 1024;     
  }
  else
    digitalWrite(2, LOW);
  */
  pwmActive = (millis()%1000 < 900);
  if ((balActive) and pwmActive) // turn off balancing for 100ms every second to give VCU change to measure properly
  {
    if (pwmCell[0] >= 0) {
      n0 += pwmCell[0]; pinMode(pinCell1, OUTPUT);
      if (n0 >= 1024) { digitalWrite(pinCell1,HIGH); n0 -= 1024; } else digitalWrite(pinCell1,LOW); }
    if (pwmCell[1] >= 0) {
      n1 += pwmCell[1]; pinMode(pinCell2, OUTPUT);
      if (n1 >= 1024) { digitalWrite(pinCell2,HIGH); n1 -= 1024; } else digitalWrite(pinCell2,LOW); }
    if (pwmCell[2] >= 0) {
      n2 += pwmCell[2]; pinMode(pinCell3, OUTPUT);
      if (n2 >= 1024) { digitalWrite(pinCell3,HIGH); n2 -= 1024; } else digitalWrite(pinCell3,LOW); }
    if (pwmCell[3] >= 0) {
      n3 += pwmCell[3]; pinMode(pinCell4, OUTPUT);
      if (n3 >= 1024) { digitalWrite(pinCell4,HIGH); n3 -= 1024; } else digitalWrite(pinCell4,LOW); }
  }
  else
  {
    pinMode(pinCell1, INPUT);
    pinMode(pinCell2, INPUT);
    pinMode(pinCell3, INPUT);
    pinMode(pinCell4, INPUT);
  }
  if (pwmActive)
    bitSet(regWarnings, 3); 
  else
    bitClear(regWarnings, 3); 
  analogWrite(pinWarnings, regWarnings*14+20);  // save a function call
  dT = micros()-tStart;
  pwmDuration += dT;
  /*
  if (testCount%2000 < 1000)
    digitalWrite(3, HIGH);
  else
    digitalWrite(3, LOW);
  */
}

// Interrupt Service Routine used for Receiving Temperature Information from VCU
ISR(PCINT2_vect)
{
  static uint32_t lastEvent = 0;
  uint32_t dT;
  static int actT = 0;
  
  if (micros()- lastEvent > 100000L)
  {
    actT = 0; // reset temp counter
    lastEvent = micros();  
    return;
  }
  // Signal LOW => Puls zu Ende => auswerten
  if (digitalRead(pinTempRX) == LOW)
  {
    dT = micros()-lastEvent;
    if ((actT < 4) && (dT < 20000L)) // max. is 200°C (only for debugging, of course too hot)
      Tcell[actT] = dT*0.01;
    actT++;
  }
  lastEvent = micros();  
}

int analogReadSlow(uint8_t pin)
{
    uint8_t i,low, high;
 
    // maximum tau in out ciruit is T = 4MOhm * 14 pF = 56 u
    ADMUX = (pin & 0x07);
    delayMicroseconds(100);

    // start the conversion by setting ADSC bit
    ADCSRA = ADCSRA | (1<<ADSC);
    
    // ADSC is cleared when the conversion finishes
    while (ADCSRA &  (1<<ADSC));
 
    low  = ADCL;
    high = ADCH;
 
    // combine the two bytes
    //return (high << 8) | low;
    return ADCW;
}

float Lin_Int_OCV(float Xin)
{
  int i,j=7;
  float ya;
  for(i=0;i<7;i++)
  {
      if (Xin>=SOCx[i] && Xin<SOCx[i+1])
      {
          j = i;
          break;
      }
  }
  ya = SOCy[j] + (Xin-SOCx[j])*(SOCy[j+1]-SOCy[j])/(SOCx[j+1]-SOCx[j]);
  return ya;
}

// input voltage 0 to 1 V is treated as SOC 0 - 100%
// translation to ~OCV by interpolation
float scaleVcell(float v)
{
  return Lin_Int_OCV(v);
}

// Scale SOC Batterie between 80 and 880 analog Value
// => cell range 20 to 220
void getRawMeasurements()
{
  static uint32_t lastMeasurement = 0;
  float V1, V2, V3, V4, Vshunt;
  int i;
  
  // only measure after at least 100ms of the last measurement
  if (millis()-lastMeasurement < 100)
    return;

  balActive = false;    // turn off PWM on analog cell pins and switch to Inputs
  delay(3);             // 100k*3,3nF = 0,33ms => 6 Tau = 2 ms + 1ms turnaround of ISR
  
  lastMeasurement = millis();

  Vshunt = analogRead(pinUshunt)*5.0/1024;

  Vraw[3] = analogRead(pinCell4); V4 = Vraw[3]*5.0/1024;
  Vraw[2] = analogRead(pinCell3); V3 = Vraw[2]*5.0/1024; 
  Vraw[1] = analogRead(pinCell2); V2 = Vraw[1]*5.0/1024;
  Vraw[0] = analogRead(pinCell1); V1 = Vraw[0]*5.0/1024;

  Ibat = (Vshunt-V1)/100e3-V1/4e6; // real up to 10 uA
  Ibat = Ibat * 0.1e6 * 400; // scale up to 400 A
  Ibat = max(min(Ibat,400),-400);  // Current: A0-A6, positive = charge battery 
  //Serial.print(Vshunt); Serial.print("  "); Serial.print(V4); Serial.print(" I:"); Serial.println(Ibat);
  Vcell[3] = scaleVcell(V4);
  Vcell[2] = scaleVcell(V3-V4);
  Vcell[1] = scaleVcell(V2-V3);
  Vcell[0] = scaleVcell(V1-V2);
  balActive = true;   // reactivate PWM
}

float getCellVoltage(int cell)
{
  if ((cell < 1) or (cell > 4))
    return -1;
  getRawMeasurements();
  
  return max(Vcell[cell-1],0);
}

float getCellTemp(int cell)
{
  if ((cell < 1) or (cell > 4))
    return -1;
  
  return Tcell[cell-1];
}

float getPackCurrent()
{
  getRawMeasurements();
  if (((Ibat >= -9) and (Ibat <= 9)) or (not BDUactive))
    return 0;
  else
    return Ibat;
}

float getPackVoltage()
{
  getRawMeasurements();
  return (Vcell[0]+Vcell[1]+Vcell[2]+Vcell[3])*25;
}

void sendDriveMode()
{
  int value;
  if (BDUactive) value = driveMode; else value = 0;
  analogWrite(pinDriveMode, value*14+20); 
} 
void sendWarnings()
{
  analogWrite(pinWarnings, regWarnings*14+20); 
} 
void sendProduceErrors()
{
  analogWrite(pinProdErrors, regErrors*14+20); 
} 

// set Balancing Mode [Cell 1-4, OFF otherwise]
void setBalancing(int cell)
{
  if ((cell < 0) or (cell > 4))
    return -1;

  // reset pwm
  pwmCell[0] = -1;
  pwmCell[1] = -1;
  pwmCell[2] = -1;
  pwmCell[3] = -1;
  if (cell == 0) // turn off balancing
    return;
  // calc balancing  
  getRawMeasurements();  
  pwmCell[cell-1] = max(Vraw[cell-1]-50,0);    // discharge cell 
  if (cell < 4)
    pwmCell[cell] = min(Vraw[cell]+50,1023);  // charge cells below accordingly
}

void setBDU_Activation(bool state)
{
  BDUactive = state; 
  sendDriveMode(); 
}

void setDriveMode(int mode)
{
  if ((mode >= 1) and (mode <= 4))  
    driveMode = mode;
  sendDriveMode(); 
}

void setWarningOvervoltage(bool state)
{
  if (state)
    bitSet(regWarnings, 0); 
  else  
    bitClear(regWarnings, 0);
  sendWarnings();
}

void setWarningUndervoltage(bool state)
{
  if (state)
    bitSet(regWarnings, 1); 
  else  
    bitClear(regWarnings, 1);
  sendWarnings();
}

void setWarningOvertemp(bool state)
{
  if (state)
    bitSet(regWarnings, 2); 
  else  
    bitClear(regWarnings, 2);
  sendWarnings();
}

void setIgnoreLimits(bool state)
{
  if (state)
    bitSet(regErrors, 0); 
  else  
    bitClear(regErrors, 0);
  sendProduceErrors();
}

void setModifySignals(bool state, bool randomize)
{
  if (state)
    bitSet(regErrors, 1); 
  else  
    bitClear(regErrors, 1);
  if (randomize)
    bitSet(regErrors, 2); 
  else  
    bitClear(regErrors, 2);
  sendProduceErrors();
}
