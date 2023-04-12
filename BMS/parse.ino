void receiveAndParseCommands()
{
  char first;
  int  value;
  while (Serial.available() > 0) // Lese Buffer solange aus bis er leer ist
  { 
    first = Serial.read();       // Lese Erstes Byte
    value = Serial.parseInt();   // Interpretiere Rest als Integer
    switch(first)
    {
      case 'B':
        if ((value >= 0) and (value <= 1))
        {
          setBDU_Activation(value);
          Serial.print("set BDU: "); Serial.println(value);
        }
        break;
      case 'D':
        if ((value >= 1) and (value <= 4))
        {
          setDriveMode(value);
          Serial.print("set Drive Mode: "); Serial.println(value);
        }
        break;
      case 'E':
        if (value == 1)
        {
          setModifySignals(true, false);
          Serial.println("turn on next error");
        }
        else if (value == 2)
        {
          setModifySignals(true, true);
          Serial.println("turn on next random error");
        }
        else
        {
          setModifySignals(false, false);
          Serial.println("turn off errors");
        }
        break;
    }
  }  
}
