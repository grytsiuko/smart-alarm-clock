#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <IRremote.h>

const int RED_PIN = 5;
const int GREEN_PIN = 10;
const int BLUE_PIN = 9;
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
const unsigned long SIGNAL_CH_MINUS = 0xFFA25D;
const unsigned long SIGNAL_CH = 0xFF629D;
const unsigned long SIGNAL_CH_PLUS = 0xFFE21D;
const unsigned long SIGNAL_VOL_MINUS = 0xFFE01F;
const unsigned long SIGNAL_EQ = 0xFF906F;
const unsigned long SIGNAL_0 = 0xFF6897;
const unsigned long SIGNAL_1 = 0xFF30CF;
const unsigned long SIGNAL_2 = 0xFF18E7;
const unsigned long SIGNAL_3 = 0xFF7A85;
const unsigned long SIGNAL_4 = 0xFF10EF;
const unsigned long SIGNAL_5 = 0xFF38C7;
const unsigned long SIGNAL_6 = 0xFF5AA5;
const unsigned long SIGNAL_7 = 0xFF42BD;
const unsigned long SIGNAL_8 = 0xFF4AB5;
const unsigned long SIGNAL_9 = 0xFF52AD;

iarduino_RTC rtc(RTC_DS1302,3,12,2);
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

const Time DEFAULT_ALARM{23, 22, 20};
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

const int MAX_FINISHED_TIME_SECONDS = 10;

const int DELAY_LENGTH = 10;

const int GRADIENT_DURATION_SECONDS = 5;
const int GRADIENT_STEP_DURATION = GRADIENT_DURATION_SECONDS * 1000 / GRADIENT_LENGTH;
const int GRADIENT_STEP_ITERATIONS = GRADIENT_STEP_DURATION / DELAY_LENGTH;

const int LCD_UPDATE_DELAY_STEPS = 20;



enum StateTitle {
    MAIN, CLOCK, ALARM, DURATION
};

enum StateType {
    NONE, TIME, INTEGER
};

const unsigned long ACTION_CLOCK = SIGNAL_CH_MINUS;
const unsigned long ACTION_ALARM = SIGNAL_CH;
const unsigned long ACTION_DURATION = SIGNAL_CH_PLUS;
const unsigned long ACTION_SUBMIT = SIGNAL_EQ;
const unsigned long ACTION_BACK = SIGNAL_VOL_MINUS;

const int STATE_INPUT_LENGTH = 6;
const int STATE_INPUT_INTEGER_MAX_LENGTH = 3;
const int STATE_INPUT_NAN = -1;

struct State {
  StateTitle title;
  StateType type;
  int input[STATE_INPUT_LENGTH];
  int step;
};




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

  void deadTimer() {
    if (!finished) {
      return;
    }
    unsigned long currTime = millis();
    if (currTime - startTime > 1000 * (MAX_FINISHED_TIME_SECONDS + GRADIENT_DURATION_SECONDS)) {
      reset();
    }
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

  unsigned long startTime;
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

  void setGradientDuration(int duration) {
    gradientDuration = duration;
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
  StateTitle lastTitle = MAIN;

  void printMainProgress(const String &status, const double progress) {
      lcd.setCursor(0, 0);
      lcd.print("                    ");
      lcd.setCursor(0, 0);
      int progressDec = progress * 100;
      lcd.print(status + " " + String(progressDec) + "%");
  }

  String padNumber(int a) {
    return a < 10 ? String("0") + String(a) : String(a);
  }

  void printMainTime(const String &currentTime) {
      lcd.setCursor(0, 1);
      lcd.print("CLOCK:    " + currentTime);
  }

  void printMainAlarm(const Time &alarmTime) {
      lcd.setCursor(0, 2);
      lcd.print("ALARM:    " + padNumber(alarmTime.hours) + ":" + padNumber(alarmTime.minutes) + ":" + padNumber(alarmTime.seconds));
  }

  void printMainDuration(const int duration) {
      lcd.setCursor(0, 3);
      lcd.print("DURATION: " + String(duration) + "s");
  }

  void printClockTitle() {
      lcd.setCursor(0, 0);
      lcd.print("Set CLOCK");
  }

  void printAlarmTitle() {
      lcd.setCursor(0, 0);
      lcd.print("Set ALARM");
  }

  void printDurationTitle() {
      lcd.setCursor(0, 0);
      lcd.print("Set DURATION");
  }

  void printIntegerInput(const State &state) {
      lcd.setCursor(0, 1);
      for (int i = 0; i < state.step; i++) {
        lcd.print(state.input[i]);
      }
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

  void update(const State &state, const String &status, const double progress, const String &currentTime, const Time &alarmTime, const int gradientDuration) {
    step++;
    if (step != LCD_UPDATE_DELAY_STEPS) {
      return;
    }

    if (state.title != lastTitle) {
      lcd.clear();
      lastTitle = state.title;
    }

    switch (state.title) {
      case MAIN:
        printMainProgress(status, progress);
        printMainDuration(gradientDuration);
        printMainAlarm(alarmTime);
        printMainTime(currentTime);
        break;
      case CLOCK:
        printClockTitle();
        break;
      case ALARM:
        printAlarmTitle();
        break;
      default:
        printDurationTitle();
        printIntegerInput(state);
        break;
    }
    step = 0;
  }
};



class StateModel {

  private:

  SettingsModel &settingsModel;
  ClockModel &clockModel;
  State state = generateInitState(MAIN);

  State generateInitState(StateTitle title) {
    switch (title) {
      case MAIN:
        return {MAIN, NONE, {0}, 0};
      case CLOCK:
        return {CLOCK, TIME, {0}, 0};
      case ALARM:
        return {ALARM, TIME, {0}, 0};
      default:
        return {DURATION, INTEGER, {0}, 0};
    }
  }

  int getNumberBySignal(unsigned long signal) {
    switch (signal) {
      case SIGNAL_0:
        return 0;
      case SIGNAL_1:
        return 1;
      case SIGNAL_2:
        return 2;
      case SIGNAL_3:
        return 3;
      case SIGNAL_4:
        return 4;
      case SIGNAL_5:
        return 5;
      case SIGNAL_6:
        return 6;
      case SIGNAL_7:
        return 7;
      case SIGNAL_8:
        return 8;
      case SIGNAL_9:
        return 9;
      default:
        return STATE_INPUT_NAN;
    }
  }

  void proceedIntegerStep(int number) {
    if (number == 0 && state.step == 0) {
      return;
    }
    if (state.step == STATE_INPUT_INTEGER_MAX_LENGTH) {
      return;
    }
    state.input[state.step] = number;
    state.step++;
  }

  void submitDuration() {
    if (state.step == 0) {
      return;
    }

    int result = 0;
    for (int i = 0; i < state.step; i++) {
      result *= 10;
      result += state.input[i];
    }
    settingsModel.setGradientDuration(result);
  }

  public:

  const State &getState() {
    return state;
  }

  void processSignal(unsigned long signal) {
    switch (state.title) {
      
      case MAIN:
        if (signal == ACTION_CLOCK) {
          state = generateInitState(CLOCK);
        }
        if (signal == ACTION_ALARM) {
          state = generateInitState(ALARM);
        }
        if (signal == ACTION_DURATION) {
          state = generateInitState(DURATION);
        }
        break;
        
      case CLOCK:
        if (signal == ACTION_BACK) {
          state = generateInitState(MAIN);
        }
        break;
        
      case ALARM:
        if (signal == ACTION_BACK) {
          state = generateInitState(MAIN);
        }
        break;
        
      case DURATION:
        if (signal == ACTION_BACK) {
          state = generateInitState(MAIN);
        }
        int number = getNumberBySignal(signal);
        if (number != STATE_INPUT_NAN) {
          proceedIntegerStep(number);
        }
        if (signal == ACTION_SUBMIT) {
          submitDuration();
          state = generateInitState(MAIN);
        }
        break;
    }
  }

  StateModel(SettingsModel &settingsModel, ClockModel &clockModel): settingsModel(settingsModel), clockModel(clockModel) {
  }
};



class Controller {

  private:

  StateModel &stateModel;
  SettingsModel &settingsModel;
  LightModel &lightModel;
  ClockModel &clockModel;
  IRModel &irModel;
  LightView &lightView;
  LcdView &lcdView;
  
  public:

  Controller(
    StateModel &stateModel, SettingsModel &settingsModel, LightModel &lightModel, ClockModel &clockModel, 
    IRModel &irModel, LightView &lightView, LcdView &lcdView
  ): stateModel(stateModel), settingsModel(settingsModel), lightModel(lightModel), clockModel(clockModel), irModel(irModel), 
     lightView(lightView), lcdView(lcdView) {
  }

  void setup() {
    Serial.begin(9600);
    lcdView.setup();
    clockModel.setup();
    irModel.setup();
  }

  void execute() {    
    unsigned long signal = irModel.get();

    stateModel.processSignal(signal);
    
    if (signal == SIGNAL_PLAY_PAUSE) {
      if (lightModel.isStarted()) {
        lightModel.reset();
      } else {
        lightModel.start();
      }
    }

    lightModel.deadTimer();

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
    lcdView.update(stateModel.getState(), lightModel.getStatus(), lightModel.getProgress(), clockModel.getCurrentTime(), settingsModel.getAlarm(), settingsModel.getGradientDuration());
  }
};



LightModel lightModel;
SettingsModel settingsModel;
ClockModel clockModel;
StateModel stateModel(settingsModel, clockModel);
IRModel irModel;
LightView lightView;
LcdView lcdView;
Controller controller(stateModel, settingsModel, lightModel, clockModel, irModel, lightView, lcdView);


void setup(){
  controller.setup();
}

void loop(){
  controller.execute();
  delay(DELAY_LENGTH);
}
