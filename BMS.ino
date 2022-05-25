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
void setBDU_Activation(bool state);
void setBalancing(int cell);



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
    unsigned long triggerTime2MinIntervall;
    unsigned long overcurrentTimeSum = 0;
    unsigned long timeIntervall10minStart;
    unsigned long triggerTimeUndervoltage;
    bool undervoltageTriggered;
    bool overvoltageTriggered;
    unsigned long triggerTimeOvervoltage;
    bool timeIntervallTriggerd;

  public:
    void checkForVoltageOutOfRange(){
      updateData();
      for(int i = 0; i<4; i++){
        if(cellVoltages[i] < 2.5){

          if(!undervoltageTriggered){
            triggerTimeUndervoltage = millis();  
            undervoltageTriggered = true;            
          } 
          

          unsigned long timedelta = millis() - triggerTimeUndervoltage;

          if(timedelta > 3000){
            setBDU_Activation(true);
            return;
          }

          setWarningUndervoltage(true);
          
          return;
        }

        if(cellVoltages[i] > 4.2){

          if(!overvoltageTriggered){
            triggerTimeOvervoltage = millis();
              overvoltageTriggered = true;            
          }
          
          unsigned long timedelta = millis() - triggerTimeUndervoltage;

          if(timedelta > 3000){
            setBDU_Activation(false);
            return;
          }
          setWarningOvervoltage(true);
          return;
        }

      }
      setWarningUndervoltage(false);
      setWarningOvervoltage(false);
      setBDU_Activation(true);
      undervoltageTriggered = false;
      overvoltageTriggered = false;
    }

    void checkForOverTemp(){
      updateData();
      for(int i=0; i++; i<4){
        Serial.println("Test");
        if(cellTemp[i] > 40){
          setWarningOvertemp(true);
          return;
        }
      }
      setWarningOvertemp(false);
    }

    void checkForOvercurrent(){
      updateData();

      if(packCurrent >= 400){

          if(!timeIntervallTriggerd){
            timeIntervall10minStart = millis();
            overcurrentTimeSum = 0; 
            timeIntervallTriggerd = true;
          } 

          if(timeIntervall10minStart >= 600000){
            timeIntervallTriggerd = false;
            return;
          }

              

          triggerTime2MinIntervall = millis();
          unsigned long buffer;

          while(packCurrent >= 400){
            updateData();
            buffer = millis() - triggerTime2MinIntervall;
            if(buffer + overcurrentTimeSum > 12000){
              setBDU_Activation(false);
              timeIntervall10minStart = 0;
              overcurrentTimeSum = 0;
              timeIntervallTriggerd = false;
              return;
            }
          }

          overcurrentTimeSum = overcurrentTimeSum + buffer;
          
      }
    } 
};

class Balancing: protected Dataclass{
  private:
    double sortedArray[4];
    int cellPosition[4] = {1,2,3,4};
    double cellDrift;
    unsigned long triggerTime2MinIntervall;

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
      int j, i, tempPosition;
      
      for(i = 0; i<4; i++) {
        for(j = i+1; j<4; j++){
          if(sortedArray[j] < sortedArray[i]){
            temp = sortedArray[i];
            tempPosition = cellPosition[i];
            sortedArray[i] = sortedArray[j];
            cellPosition[i] = cellPosition[j];
            sortedArray[j] = temp;
            cellPosition[j] = tempPosition;
          }
        }
      }
    } 

    public:
      void checkForBalancing(){
        updateData();
        calculateCellDrift();
        if(sortedArray[3] > 4.1 && cellDrift >= 0.3){
          setBalancing(cellPosition[3]);
          return; 
        }         
        setBalancing(0);
        
    }
};


Safety safetyController;

Balancing balancer;

void setup() {
  setupBSW();
  setBDU_Activation(true); 
}

void loop() { 

  static uint32_t oldTimeSafety = millis();
  static uint32_t oldtimeBalancing = millis();
     // schaltet BDU ein           // 1-Cycle Test 2-Slow Driver 3-Fast Driver 4-Power Mode
  receiveAndParseCommands();   // Empfängt Befehle über den Serial Monitor und führt diese aus
  
  showMeasurementValues();   // Stellt Messwerte numerisch dar
  drawMeasurementCurves(10); // Messkurven - Parameter defines Minutes for full scale of X-Axis
  //showOCVcurve();


  if((millis()-oldTimeSafety) > 100){
      oldTimeSafety = millis();
      safetyController.checkForVoltageOutOfRange();
      safetyController.checkForOverTemp();
      safetyController.checkForOvercurrent();
      
  }

  if(millis()-oldtimeBalancing > 1000){
    oldtimeBalancing = millis();
    balancer.checkForBalancing();
  }
}