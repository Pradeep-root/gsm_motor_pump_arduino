
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
SoftwareSerial GSM(3, 2); // RX, TX
DeserializationError error;
StaticJsonDocument<200> doc;

enum _parseState {
  PS_DETECT_MSG_TYPE,
  PS_IGNORING_COMMAND_ECHO,
  PS_HTTPACTION_TYPE,
  PS_HTTPACTION_RESULT,
  PS_HTTPACTION_LENGTH,
  PS_HTTPREAD_LENGTH,
  PS_HTTPREAD_CONTENT
};

byte parseState = PS_DETECT_MSG_TYPE;
char buffer[100];
byte pos = 0;
char json[] =
     "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
const char* title;

int contentLength = 0;

void resetBuffer() {
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

/**
 * Parse the AT text and on the basis of the response set the enum status
 * @param b byte data came from the gsm serial response
 */
void parseATText(byte b) {
  buffer[pos++] = b;
  if ( pos >= sizeof(buffer) )
    resetBuffer(); // just to be safe

/*
   // Detailed debugging
   Serial.println();
   Serial.print("state = ");
   //Serial.println(state);
   Serial.print("b = ");
   Serial.println(b);
   Serial.print("pos = ");
   Serial.println(pos);
   Serial.print("buffer = ");
   Serial.println(buffer);*/

  switch (parseState) {
  case PS_DETECT_MSG_TYPE:
    {
      if ( b == '\n' )
        resetBuffer();
      else {
        if ( pos == 3 && strcmp(buffer, "AT+") == 0 ) {
          parseState = PS_IGNORING_COMMAND_ECHO;
        }
        else if ( b == ':' ) {
          //Serial.print("Checking message type: ");
          //Serial.println(buffer);
          if ( strcmp(buffer, "+HTTPACTION:") == 0 ) {
            Serial.println("Received HTTPACTION");
            parseState = PS_HTTPACTION_TYPE;
          }
          else if ( strcmp(buffer, "+HTTPREAD:") == 0 ) {
            Serial.println("Received HTTPREAD");
            parseState = PS_HTTPREAD_LENGTH;
          }
          resetBuffer();
        }
      }
    }
    break;

  case PS_IGNORING_COMMAND_ECHO:
    {
      if ( b == '\n' ) {
        Serial.print("Ignoring echo: ");
        Serial.println(buffer);
        parseState = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;

  case PS_HTTPACTION_TYPE:
    {
      if ( b == ',' ) {
        Serial.print("HTTPACTION type is ");
        Serial.println(buffer);
        parseState = PS_HTTPACTION_RESULT;
        resetBuffer();
      }
    }
    break;

  case PS_HTTPACTION_RESULT:
    {
      if ( b == ',' ) {
        Serial.print("HTTPACTION result is ");
        Serial.println(buffer);
        parseState = PS_HTTPACTION_LENGTH;
        resetBuffer();
      }
    }
    break;

  case PS_HTTPACTION_LENGTH:
    {
      if ( b == '\n' ) {
        Serial.print("HTTPACTION length is ");
        Serial.println(buffer);
        // now request content
        GSM.print("AT+HTTPREAD=0,");
        GSM.println(buffer);
        parseState = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;

  case PS_HTTPREAD_LENGTH:
    {
      if ( b == '\n' ) {
        contentLength = atoi(buffer);
        Serial.print("HTTPREAD length is ");
        Serial.println(contentLength);
        Serial.print("HTTPREAD content: ");
        parseState = PS_HTTPREAD_CONTENT;
        resetBuffer();
      }
    }
    break;

  case PS_HTTPREAD_CONTENT:
    {
      // for this demo I'm just showing the content bytes in the serial monitor
      Serial.write(b);
      contentLength--;

      if (contentLength <= 0 ) {
        // all content bytes have now been read
        error = deserializeJson(doc, json);
        if(error){
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }
        title = doc["sensor"];
        Serial.print(title);
        parseState = PS_DETECT_MSG_TYPE;
        resetBuffer();
      }
    }
    break;
  }
}

/**
 * Send AT command and data to gsm with required delay
 * @param msg    AT command with data
 * @param waitMs delay reqired for response
 */
void sendATgsm(const char* msg, int waitMs = 500) {
  GSM.println(msg); // sending the AT command to gsm module
  delay(waitMs);
  while(GSM.available()) {
    parseATText(GSM.read());
  }
}


void setup()
{
  GSM.begin(9600);
  Serial.begin(9600);
  sendATgsm("AT+SAPBR=3,1,\"APN\",\"airtelgprs.com\"");
  sendATgsm("AT+SAPBR=1,1",3000);
  sendATgsm("AT+HTTPINIT");
  sendATgsm("AT+HTTPPARA=\"CID\",1");
  sendATgsm("AT+HTTPPARA=\"URL\",\"http://www.recipepuppy.com/api/?i=onions,garlic&q=omelet\"");
  sendATgsm("AT+HTTPACTION=0");
}

void loop()
{
  while(GSM.available()) {
   parseATText(GSM.read());
  }
}
