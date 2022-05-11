// Include necessary libraries
#include <SPI.h>              // SPI communication library
#include <Adafruit_ILI9341.h> // Display library
#include <TouchScreen.h>      // Display touch functionality

extern volatile uint32_t pwmDuration;

void setup() 
{
  setupBSW();
}

void loop() 
{ 
  setBDU_Activation(true);   // schaltet BDU ein
  setDriveMode(1);           // 1-Cycle Test 2-Slow Driver 3-Fast Driver 4-Power Mode
  receiveAndParseCommands();   // Empfängt Befehle über den Serial Monitor und führt diese aus
  
  showMeasurementValues();   // Stellt Messwerte numerisch dar
  drawMeasurementCurves(10); // Messkurven - Parameter defines Minutes for full scale of X-Axis
  //showOCVcurve();          // Stellt OCV Kurve der Li-Ionen Zellen dar
}
