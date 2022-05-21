// Include necessary libraries
#include <SPI.h>              // SPI communication library
#include <Adafruit_ILI9341.h> // Display library
#include <TouchScreen.h>      // Display touch functionality

extern volatile uint32_t pwmDuration;

// getter prototypes
float getCellVoltage(int cell);
float getCellTemp(int cell);
float getPackCurrent();
float getPackVoltage();
void setBalancing();

// VCU setter Prototypes
void setWarningUndervoltage(bool state);
void setWarningOvervoltage(bool state);
void setWarningOvertemp(bool state);
void setWarningOvertemp(bool state);



class Dataclass{
  protected:
    double cellVoltages[4];
    double cellTemp[4];
    double packCurrent;
    double packVoltage;

    void updateData(){
      updateCellVoltages();  
      updateCellTemp();
      updatePackCurrent();
      updatePackVoltage();

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

class Safety:protected Dataclass{
  private:

  public:

    void checkForVoltageOutOfRange(){
      updateData();


      for(int i = 0; i<4; i++){
        if(cellVoltages[i] < 2.5){

          setWarningUndervoltage(true);
          return;
        }
        if(cellVoltages[i] > 4.2){
          setWarningOvervoltage(true);
          return;
        }        
      }
      setWarningUndervoltage(false);
      setWarningOvervoltage(false);
    }

    void checkForOverTemp(){
      updateData();
      for(int i=0; i++; i<4){
        if(cellTemp[i] > 50 || cellTemp[i] < 15){
          setWarningOvertemp(true);
          return;
        }
      }
      setWarningOvertemp(false);
    } 
};


class Balancing: protected Dataclass{
  private:
    double sortedArray[4];
    double cellDrift;
    unsigned long triggerTime;

    void calculateCellDrift(){
        copy();
        sort();
        cellDrift = sortedArray[3] - sortedArray[0];
    }

    void copy(){
      for(int i=0; i<4; i++){
        sortedArray[i]=cellVoltages[i];
      }
    }

    void sort(){
      double temp;
      int j, i;
      
      for(i = 0; i<10; i++) {
        for(j = i+1; j<10; j++){
          if(sortedArray[j] < sortedArray[i]){
            temp = sortedArray[i];
            sortedArray[i] = sortedArray[j];
            sortedArray[j] = temp;
          }
        }
      }
    } 

    public:
      void checkForBalancing(){
        calculateCellDrift();

        while(cellDrift > 0.2){
          calculateCellDrift();
        }
      }
};


Safety safetyController;

Balancing balancer;

void setup() {
  setupBSW();
}

void loop() { 
  static uint32_t oldtime = millis();
  setBDU_Activation(true);   // schaltet BDU ein
  setDriveMode(1);           // 1-Cycle Test 2-Slow Driver 3-Fast Driver 4-Power Mode
  receiveAndParseCommands();   // Empfängt Befehle über den Serial Monitor und führt diese aus
  
  showMeasurementValues();   // Stellt Messwerte numerisch dar
  drawMeasurementCurves(10); // Messkurven - Parameter defines Minutes for full scale of X-Axis
  //showOCVcurve();

  safetyController.checkForVoltageOutOfRange();
  safetyController.checkForOverTemp();

  if((millis()-oldtime)>10){
      oldtime = millis();
      Serial.println("Sucess"); 
  }


  //setWarningUndervoltage(true);
  
  delay(1);

           // Stellt OCV Kurve der Li-Ionen Zellen dar
}
