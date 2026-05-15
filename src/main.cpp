#include "Particle.h"
#include "Grove_ChainableLED.h"
#include "Grove-Ultrasonic-Ranger.h"
#include "Grove_4Digit_Display.h"
#include "Adafruit_DHT_Particle.h"

SYSTEM_MODE(AUTOMATIC);
SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler(LOG_LEVEL_INFO);

#define MAX_HEIGHT 4 
#define MAX_TEMP 30 
#define MAX_HUMID 70 

int therapy_hours[] = {9, 12, 17}; 
String patient_name = "Andrea Diano";
int temperature, humidity;


bool therapy_done = false;
bool therapy_active = false; 
bool waiting_for_closure = false; 
bool actual;
bool last; 
long distance;


const unsigned long ENV_CHECK_INTERVAL = 300000; 
const unsigned long ENV_ALERT_COOLDOWN = 1200000; 
const unsigned long COVER_ALERT_COOLDOWN = 300000; 
const unsigned long THERAPY_ALERT_COOLDOWN = 600000; 

unsigned long lastMessageTime = THERAPY_ALERT_COOLDOWN;
unsigned long lastEnvCheckTime = ENV_CHECK_INTERVAL;      
unsigned long lastEnvAlertTime = ENV_ALERT_COOLDOWN;      
unsigned long lastCoverAlertTime = COVER_ALERT_COOLDOWN;  

#define BUZZER A0
#define NUM_LEDS  1
ChainableLED leds(A4, A5, NUM_LEDS);

Ultrasonic ultrasonic(D4);
TM1637 display(D1, D0);

#define DHTPIN D2      
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);


void live_clock(){

  // Show the current time on the 4-digit display
  int hour = Time.hour();
  int minute = Time.minute();
  int second = Time.second();

  Serial.printlnf("Time is %02d:%02d:%02d", hour, minute, second);

  display.display(0, hour / 10);
  display.display(1, hour % 10);
  display.display(2, minute / 10);
  display.display(3, minute % 10);
  if (second % 2 == 0) {
      display.point(POINT_ON);
  } else {
      display.point(POINT_OFF);
  }
  delay(500);
}

bool is_therapy_time(){
  // Check whether the current hour matches a therapy schedule
  int current_hour = Time.hour();
  if(current_hour == therapy_hours[0] || current_hour == therapy_hours[1] || current_hour == therapy_hours[2])
    return true;
  else
    return false;
}

bool is_open(){
  // Measure the hatch distance and determine whether it is open or closed

  long temp_distance = ultrasonic.MeasureInCentimeters();
  Serial.printlnf("Measured distance: %ld cm", temp_distance);

  if(temp_distance > MAX_HEIGHT)
  {
    long confirm_distance = ultrasonic.MeasureInCentimeters();
    if(confirm_distance > MAX_HEIGHT)
    {
      distance = confirm_distance;
      Serial.printlnf("Confirmed distance: %ld cm", confirm_distance);
      return true;
    }
    else
    {
      Serial.printlnf("False alarm");
      return false;
    }
  }
  else
  {
    distance = temp_distance;
    return false;
  }
}

void buzz_for_therapy(){
  // Emit a buzzer signal during the therapy window
      int minutes = Time.minute();
      if(minutes % 5 == 0)
      {
        tone(BUZZER, 1800); 
        delay(200);         
        noTone(BUZZER);
        
        delay(100);         

        tone(BUZZER, 1800); 
        delay(400);         
        noTone(BUZZER);

        delay(250);
      }
  } 

void BlinkStatusLED(int timedelay, float hue ){
  // Blink the status LED with a specific color
  leds.setColorHSB(0, hue, 1.0, 0.5);
  delay(timedelay);
  leds.setColorHSB(0, hue, 0.0, 0.0); // Off
  delay(timedelay);
}

void alert_Patient_on_therapy() {
  // Warn the patient when therapy is late
  if(millis() - lastMessageTime < THERAPY_ALERT_COOLDOWN) {
    return;
  }
  lastMessageTime = millis();
  int current_hour = Time.hour();
  int current_minute = Time.minute();
  String message = "Hi " + patient_name + ", it's time for your therapy and you are late by " + String(current_minute) + " minutes compared to " + String(current_hour) + ":00.";  
  Particle.publish("Therapy Alert", message, PRIVATE);
}

void publish_Drive () {
  // Publish to the cloud that the patient opened the hatch for therapy
  String message = "The patient " + patient_name + " opened the SMART BOX hatch to take therapy.";
  Particle.publish("Drive Alert", message, PRIVATE);
}

void alert_Patient_on_cover (int c) {
  // Warn the patient in two cases: 0 = hatch open outside schedule, 1 = hatch not closed properly at therapy start
  if(millis() - lastCoverAlertTime < COVER_ALERT_COOLDOWN) {
    return;
  }
  lastCoverAlertTime = millis();
  int current_hour = Time.hour();
  String message;
  switch(c)
  {
    case 0:
    message = "Hi " + patient_name + ", the SMART BOX hatch is still open outside the therapy window. (Detected distance: " + String(distance) + " cm). Please close it.";
      break;
    case 1:
      message = "Hi " + patient_name + ", you left the hatch open during the last therapy, so please close and reopen it to log the therapy correctly.";
      break;
  }
  Particle.publish("Cover Alert", message, PRIVATE);
}

void alert_Patient_on_conditions() {
  // Warn the patient if environmental conditions are not optimal
  String message = "Hi " + patient_name + ", BOX ENVIRONMENT ALERT: Temp " + 
                   String(temperature) + " C, Humidity " + String(humidity) + "%.";
  Particle.publish("Environment Alert", message, PRIVATE);
  Serial.println("Alert message sent!");
}

void CheckEnvironmentConditions() {
  // Check temperature and humidity and alert if thresholds are exceeded
  temperature = (int)dht.getTempCelcius();
  humidity = (int)dht.getHumidity();
  if (millis() - lastEnvCheckTime < ENV_CHECK_INTERVAL) {
    return; 
  }
  lastEnvCheckTime = millis();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("DHT sensor read error!");
    return;
  }

  bool tempAlarm = (temperature > MAX_TEMP);
  bool humAlarm = (humidity > MAX_HUMID); 

  if (tempAlarm || humAlarm) {
    BlinkStatusLED(300, 0.17); 
    if(tempAlarm)
      Serial.printlnf("Alert: High temp! %.1f C", temperature);
    if(humAlarm)
      Serial.printlnf("Alert: Critical humidity! %.1f %%", humidity);

    if (millis() - lastEnvAlertTime > ENV_ALERT_COOLDOWN) {
      alert_Patient_on_conditions();     
      lastEnvAlertTime = millis();       
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  leds.init();
  display.init();
  display.set(BRIGHT_TYPICAL); 
  Time.zone(+1); 
  
  display.point(POINT_ON);
  dht.begin();
  Particle.variable("BoxTemp", temperature);
  Particle.variable("BoxHumid", humidity);
  Particle.variable("Distance", distance);
}

void loop() {
  
  live_clock(); // Update the clock on the display
  actual = is_open(); // Check whether the cover is open or closed
  CheckEnvironmentConditions(); // Check environmental conditions

  // Check Wi-Fi connection
  if(Particle.connected()) 
    leds.setColorHSB(0, 0.6, 1.0, 0.5); 
  else
    BlinkStatusLED( 300, 0.6);
  
  // Check if it is therapy time and whether therapy has already been done
  if(is_therapy_time() && !therapy_done) {
      if (!therapy_active) {
          therapy_active = true; 
          // Do a single check of the hatch status at therapy start
          if(actual) 
            waiting_for_closure = true; 
          else 
            waiting_for_closure = false;
          
      }
  }


  if(therapy_active) 
  {
    bool can_continue = true; 

    if (waiting_for_closure) {
      // If the hatch is open at therapy start, wait for it to close while warning the patient
        if (actual) {
             buzz_for_therapy();
             alert_Patient_on_cover(1); 
             BlinkStatusLED(200, 0.0);  
             can_continue = false;   

        } 
        else {
          // Hatch closed correctly, we can proceed with therapy logging
             Serial.println("Hatch closed correctly. System armed.");
             waiting_for_closure = false; 
        }
    }

    if (can_continue) 
    {
        if(!actual){
          // Hatch is closed, therapy waits for opening
          Serial.println("THERAPY ACTIVE: Waiting for the patient...");
          BlinkStatusLED(500, 0.0); 
          if(is_therapy_time())
          {
            buzz_for_therapy();
          }
          alert_Patient_on_therapy();
        }
        else if(actual && !last){
          // Hatch was just opened, therapy completed
          Serial.println("Patient opened the hatch for therapy!");
          BlinkStatusLED(500, 0.6); 
          
          therapy_active = false;
          therapy_done = true;   
          
          publish_Drive();
        }
    }
  }
  
  if(!is_therapy_time()){
    therapy_done = false; // When we exit the therapy window, reset the flag
  }

  if(!therapy_active && actual){
    // Hatch is open outside therapy time, warn the patient
    Serial.println("Hatch must be closed (Outside schedule).");
    BlinkStatusLED(300, 0.83); 
    alert_Patient_on_cover(0);
  }

  last = actual; 
}