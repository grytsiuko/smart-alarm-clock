#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <IRremote.h>

const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 5;
const int RECV_PIN = 4;

/*
 * FFA25D CH- 
 * FF629D CH
 * FFE21D CH+
 * FF22DD PREV
 * FF02FD NEXT
 * FFC23D PLAY/PAUSE
 * FFE01F VOL-
 * FFA857 VOL+
 * FF906F EQ
 * FF6897 0
 * FF9867 100+
 * FFB04F 200+
 * FF30CF 1
 * FF18E7 2
 * FF7A85 3
 * FF10EF 4
 * FF38C7 5
 * FF5AA5 6
 * FF42BD 7
 * FF4AB5 8
 * FF52AD 9
 */

const unsigned long SIGNAL_PLAY_PAUSE = 0xFFC23D;

iarduino_RTC rtc(RTC_DS1302,6,7,8);
LiquidCrystal_I2C lcd(0x27,16,2);
IRrecv irrecv(RECV_PIN);

decode_results results;

struct Color {
  int red;
  int green;
  int blue;
};

struct Time {
  int hours;
  int minutes;
  int seconds;
};

const Time DEFAULT_ALARM{23, 22, 30};
const int ALARM_START_THRESHOLD_SECONDS = 2;

const int GRADIENT_LENGTH = 9;
const Color GRADIENT_SEQUENCE[] = {
  {0, 0, 0},
  {0, 0, 10},
  {100, 0, 0},
  {200, 0, 0},
  {200, 10, 10},
  {200, 30, 30},
  {200, 70, 70},
  {200, 100, 100},
  {200, 200, 200}
};

const int DELAY_LENGTH = 10;

const int GRADIENT_DURATION_SECONDS = 20;
const int GRADIENT_STEP_DURATION = GRADIENT_DURATION_SECONDS * 1000 / GRADIENT_LENGTH;
const int GRADIENT_STEP_ITERATIONS = GRADIENT_STEP_DURATION / DELAY_LENGTH;

const int LCD_UPDATE_DELAY_STEPS = 20;



class LightModel {
  
  private:

  bool started;
  bool finished;
  Color color;
  unsigned long startTime;
  double progress;

  public:

  LightModel() {
    reset();
  }

  const Color &getColor() {
    return color;
  }

  bool isFinished() {
    return finished;
  }

  bool isStarted() {
    return started;
  }

  void reset() {
    started = false;
    finished = false;
    progress = 0;
    color = GRADIENT_SEQUENCE[0];
  }

  void start() {
    reset();
    started = true;
    this->startTime = millis();
  }

  void update(int gradientDuration) {
    if (!started || finished) {
      return;
    }

    unsigned long currTime = millis();
    progress = (currTime - startTime) / (gradientDuration * 1000.);

    if (progress >= 1) {
      progress = 1;
      finished = true;
      color = GRADIENT_SEQUENCE[GRADIENT_LENGTH - 1];
      return;
    }
    
    int step = floor((GRADIENT_LENGTH - 1) * progress);
    double stepPart = 1. / GRADIENT_LENGTH;
    double stepStart = step * stepPart;
    double stepEnd = stepStart + stepPart;
    double iterationProgress = (progress - stepStart) / (stepEnd - stepStart);
    

    const Color currStepColor = GRADIENT_SEQUENCE[step];
    const Color nextStepColor = GRADIENT_SEQUENCE[step + 1];
    const int newRed = filterColor(currStepColor.red + (nextStepColor.red - currStepColor.red) * iterationProgress);
    const int newGreen = filterColor(currStepColor.green + (nextStepColor.green - currStepColor.green) * iterationProgress);
    const int newBlue = filterColor(currStepColor.blue + (nextStepColor.blue - currStepColor.blue) * iterationProgress);
    color = {newRed, newGreen, newBlue};
  }

  int filterColor(int value) {
    if (value < 0) {
      return 0;
    }
    if (value > 255) {
      return 255;
    }
    return value;
  }

  double getProgress() {
    return progress;
  }

  String getStatus() {
    if (finished) {
      return "+";
    }
    if (started) {
      return "-";
    }
    return ".";
  }
};


class ClockModel {

  private:

  int timeToSeconds(const Time &t) {
    long hours = t.hours;
    long minutes = hours * 60 + t.minutes;
    long seconds = minutes * 60 + t.seconds;
    return seconds;
  }

  long secondsInDay = 24 * 60 * 60;

  public:

  ClockModel() = default;


  void setup() {
    rtc.begin();
    rtc.settime(10, 22, 23, 13, 11, 21, 7); 
  }

  String getCurrentTime() {
    return rtc.gettime("H:i:s");
  }

  unsigned long getUnix() {
    return rtc.Unix;
  }

  bool shouldGradientStart(const Time &alarm, int gradientDuration) {
    long alarmSeconds = timeToSeconds(alarm);
    long gradientStartSeconds = (alarmSeconds - gradientDuration + secondsInDay) % secondsInDay;
    
    long currentSeconds = timeToSeconds({rtc.hours + (rtc.midday ? 12 : 0), rtc.minutes, rtc.seconds});
    long differenceSeconds = abs(currentSeconds - gradientStartSeconds);
    
    return differenceSeconds <= ALARM_START_THRESHOLD_SECONDS || differenceSeconds >= (secondsInDay - ALARM_START_THRESHOLD_SECONDS);
  }
};


class IRModel {

  public:

  IRModel() = default;


  void setup() {
    irrecv.enableIRIn();
    irrecv.blink13(true);
  }

  unsigned long get() {
    if (irrecv.decode(&results)){
      Serial.println(results.value, HEX);
      unsigned long value = results.value;
      irrecv.resume();
      return value;
    }
    return 0;
  }
};


class SettingsModel {

  private:

  Time alarm = DEFAULT_ALARM;
  int gradientDuration = GRADIENT_DURATION_SECONDS;

  public:

  SettingsModel() = default;


  void setup() {
    irrecv.enableIRIn();
    irrecv.blink13(true);
  }

  const Time &getAlarm() {
    return alarm;
  }

  int getGradientDuration() {
    return gradientDuration;
  }
};




class LightView {

  public:

  LightView() = default;

  void update(const Color &color) {
    analogWrite(RED_PIN, color.red);
    analogWrite(GREEN_PIN, color.green);
    analogWrite(BLUE_PIN, color.blue);
  }
};




class LcdView {

  private:

  int step;

  void printProgress(const String &status, const double progress) {
      lcd.setCursor(0, 0);
      lcd.print("                    ");
      lcd.setCursor(0, 0);
      int progressDec = progress * 100;
      lcd.print(status + " " + String(progressDec) + "%");
  }

  String padNumber(int a) {
    return a < 10 ? String("0") + String(a) : String(a);
  }

  void printTime(const String &currentTime) {
      lcd.setCursor(0, 1);
      lcd.print("CLOCK:    " + currentTime);
  }

  void printAlarm(const Time &alarmTime) {
      lcd.setCursor(0, 2);
      lcd.print("ALARM:    " + padNumber(alarmTime.hours) + ":" + padNumber(alarmTime.minutes) + ":" + padNumber(alarmTime.seconds));
  }

  void printDuration(const int duration) {
      lcd.setCursor(0, 3);
      lcd.print("DURATION: " + String(duration) + "s");
  }

  public:

  LcdView() {
    step = 0;
  }
  
  void setup(){
    lcd.init();   
    lcd.begin(20, 4);                  
    lcd.backlight();
  }

  void update(const String &status, const double progress, const String &currentTime, const Time &alarmTime, const int gradientDuration) {
    step++;
    if (step == LCD_UPDATE_DELAY_STEPS) {
      printProgress(status, progress);
      printDuration(gradientDuration);
      printAlarm(alarmTime);
      printTime(currentTime);
      step = 0;
    }
  }
};




class Controller {

  private:

  SettingsModel settingsModel;
  LightModel lightModel;
  ClockModel clockModel;
  IRModel irModel;
  LightView lightView;
  LcdView lcdView;
  
  public:

  Controller(
    const SettingsModel &settingsModel, const LightModel &lightModel, const ClockModel &clockModel, const IRModel &irModel, 
    const LightView &lightView, const LcdView &lcdView
  ) {
    this->settingsModel = settingsModel;
    this->lightModel = lightModel;
    this->clockModel = clockModel;
    this->irModel = irModel;
    this->lightView = lightView;
    this->lcdView = lcdView;
  }

  void setup() {
    Serial.begin(9600);
    lcdView.setup();
    clockModel.setup();
    irModel.setup();
  }

  void execute() {
    unsigned long signal = irModel.get();
    if (signal == SIGNAL_PLAY_PAUSE) {
      if (lightModel.isStarted()) {
        lightModel.reset();
      } else {
        lightModel.start();
      }
    }

    if (!lightModel.isFinished()) {
      if (!lightModel.isStarted()) {
        if (clockModel.shouldGradientStart(settingsModel.getAlarm(), settingsModel.getGradientDuration())) {
          lightModel.start();
        }
      } else {
        lightModel.update(settingsModel.getGradientDuration());
      }
    }
    
    lightView.update(lightModel.getColor());
    lcdView.update(lightModel.getStatus(), lightModel.getProgress(), clockModel.getCurrentTime(), settingsModel.getAlarm(), settingsModel.getGradientDuration());
  }
};



SettingsModel settingsModel;
LightModel lightModel;
ClockModel clockModel;
IRModel irModel;
LightView lightView;
LcdView lcdView;
Controller controller(settingsModel, lightModel, clockModel, irModel, lightView, lcdView);


void setup(){
  controller.setup();
}

void loop(){
  controller.execute();
  delay(DELAY_LENGTH);
}
