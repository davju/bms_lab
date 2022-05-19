// Include necessary libraries
#include <SPI.h>              // SPI communication library
#include <Adafruit_ILI9341.h> // Display library
#include <TouchScreen.h>      // Display touch functionality

extern volatile uint32_t pwmDuration;

float getCellVoltage(int cell);
float getCellTemp(int cell);
float getPackCurrent();
float getPackVoltage();
void setBalancing();


class Dataclass{
  protected:
    double cellVoltages[4];
    double cellTemp[4];
    double packCurrent;
    double packVoltage;

    void updateData(){
      updateCellVoltages();      
  }

  private:
    void updateCellVoltages(){
      for(int i = 0; i<4;i++){
       cellVoltages[i] = getCellVoltage(i+1);
      }
    }

    void updateCellTemp(){
      for(int i = 0; i<4;i++){
        cellTemp[i] = getCellTemp(i+1);
      }
    }

    void updatePackCurrent(){
        packCurrent = getPackCurrent();
    }

    void updatePackVoltage(){
        packVoltage = getPackVoltage();
    }
};

class Safety:Dataclass {
  private:
    

  public:

    void checkForVoltageOutOfRange(){
    }
};

class Balancing:Dataclass{

};

Dataclass data;


void setup() {
  setupBSW();
  

}

void loop() { 
  setBDU_Activation(true);   // schaltet BDU ein
  setDriveMode(1);           // 1-Cycle Test 2-Slow Driver 3-Fast Driver 4-Power Mode
  receiveAndParseCommands();   // Empfängt Befehle über den Serial Monitor und führt diese aus
  
  showMeasurementValues();   // Stellt Messwerte numerisch dar
  //drawMeasurementCurves(10); // Messkurven - Parameter defines Minutes for full scale of X-Axis
  showOCVcurve();
           // Stellt OCV Kurve der Li-Ionen Zellen dar
}
