void showMeasurementValues()
{
  static uint32_t lastCall = 0;
  int i;

  // do not call more often than once per second
  if (millis()-lastCall < 1000)
    return;

  lastCall = millis();
  // write Cell Voltages on screen
  for (i = 1; i <= 4; i++)
  {
    writeText(i*68-52, 4, 2, "V"+String(i), ILI9341_WHITE);
    writeText(i*68-66, 22, 2, String(getCellVoltage(i),2), colCell(i));
    writeText(i*68-66, 40, 2, strLen(String(getCellTemp(i),1),4), colTemp(i));
  }
  // write pack current on screen
  writeText(280,  4, 2, "I/A",ILI9341_LIGHTGREY);
  writeText(270, 22, 2, strLen(String(getPackCurrent(),0),4),ILI9341_BLUE);
}

void drawMeasurementCurves(uint16_t fullScreenDuration)
{
  static int tx  = 0; static float lastI=0; static uint32_t lastCall = 0;
  int i; float I;
  int mX = 319, tS = 10;
  int yV = 183; float dV = 25.0; int mVo = yV-4.2*dV; // coordinates for Voltage-Axes
  int mVu = yV-2.5*dV; // lower voltage level
  int yT = 239; float dT = 0.70; int mT = yT- 60*dT; // coordinates for Temperature-Axes
  int yI = 159; float dI = 0.07; int sI = dI*400;    // coordinates for Current-Axes
  
  // increase and reset + clear screen @ overflow
  tx++; if (tx/tS >= 319) {fillScreen(ILI9341_BLACK); tx = 0; }

  // draw Axes
  drawLine(0,mVu,294,mVu,ILI9341_DARKGREY); writeText(0, mVu-9, 1, "2.5V",colCell(3));
  drawLine(0,mVo, mX,mVo,ILI9341_DARKGREY); writeText(0, mVo-9, 1, "4.2V",colCell(3));

  drawLine( 0,yI,mX,yI,rgb565(0,0,150)); writeText(319-12, yI-9, 1, "0A",ILI9341_BLUE);
  drawLine( 0,yI-sI,mX,yI-sI,rgb565(0,0,100)); writeText(319-24, yI-9-sI, 1, "400A",ILI9341_BLUE);
  drawLine(20,yI+sI,mX,yI+sI,rgb565(0,0,100)); writeText(319-30, yI-9+sI, 1, "-400A",ILI9341_BLUE);

  drawLine(0,yT,mX,yT,ILI9341_DARKGREY); writeText(0, yT-9, 1,  "0C",colTemp(3));
  drawLine(0,mT,mX,mT,ILI9341_DARKGREY); writeText(0, mT-9, 1, "60C",colTemp(3));

  // draw Cell Voltages and Temperatures
  for (i = 1; i <= 4; i++)
  {
    drawPixel(tx/tS, yV-dV*max(min(getCellVoltage(i),5.0),2.2), colCell(i));
    drawPixel(tx/tS, yT-dT*getCellTemp(i), colTemp(i));
  }
  // draw Current
  //drawPixel(tx/tS, yI-dI*getPackCurrent(), ILI9341_BLUE);
  I = getPackCurrent();
  drawLine((tx-1)/tS,yI-dI*lastI,tx/tS,yI-dI*I,ILI9341_BLUE);
  lastI = I;

  // balancing pwm is active
  if (pwmCell[0]+pwmCell[1]+pwmCell[2]+pwmCell[3] > -4) drawPixel(tx/tS, 238, ILI9341_YELLOW); // 158
  // overvoltage
  if (regWarnings & 1) drawPixel(tx/tS, 77, ILI9341_RED);
  // undervoltage
  if (regWarnings & 2) drawPixel(tx/tS, 121, ILI9341_RED);
  // temp
  if (regWarnings & 4) drawPixel(tx/tS,  mT-1, ILI9341_RED);
  
  // wait for "dur" ms so that time for full screen matches fullScreenDuration in mins
  float dur = 60.0*fullScreenDuration/320.0/tS*1000; // duration in ms per call
  while (millis()-lastCall < dur)
    delay(1);
  lastCall = millis();
}

void showOCVcurve()
{
  float x, U;
  int yV = 183; float dV = 25.0; int mVo = yV-4.2*dV; // coordinates for Voltage-Axes

  for (x = 0; x < 1.3; x += 0.0015)
  {
    U = scaleVcell(x);
    if (U <= 4.2)
      drawPixel(round(x*100)+189,yV-dV*U, 0xFF00);
    else
      drawPixel(round(x*100)+189,yV-dV*U, ILI9341_RED);
  }
}
