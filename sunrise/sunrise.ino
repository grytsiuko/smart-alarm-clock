#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <iarduino_RTC.h>
#include <IRremote.h>

const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;
const int RECV_PIN = 4;

iarduino_RTC time(RTC_DS1302,6,7,8);
LiquidCrystal_I2C lcd(0x27,16,2);
IRrecv irrecv(RECV_PIN);

decode_results results;

struct Color {
  int red;
  int green;
  int blue;
};

const int GRADIENT_LENGTH = 10;
const Color GRADIENT_SEQUENCE[] = {
  {0, 0, 0},
  {0, 0, 10},
  {100, 0, 0},
  {200, 0, 0},
  {255, 0, 0},
  {255, 50, 50},
  {255, 100, 100},
  {255, 150, 150},
  {255, 200, 200},
  {255, 255, 255}
};

const int DELAY_LENGTH = 10;

const long GRADIENT_DURATION = 10L * 1000;
const int GRADIENT_STEP_DURATION = GRADIENT_DURATION / GRADIENT_LENGTH;
const int GRADIENT_STEP_ITERATIONS = GRADIENT_STEP_DURATION / DELAY_LENGTH;

const int LCD_UPDATE_DELAY_STEPS = 50;



class LightModel {
  
  private:

  bool started;
  bool finished;
  int step;
  int iteration;
  Color color;

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
    step = 0;
    iteration = 0;
    color = GRADIENT_SEQUENCE[0];
  }

  void start() {
    started = true;
    finished = false;
    step = 0;
    iteration = 0;
    color = GRADIENT_SEQUENCE[0];
  }

  void update() {
    if (!started || finished) {
      return;
    }

    iteration++;
    if (iteration == GRADIENT_STEP_ITERATIONS) {
      step++;
      iteration = 0;
    }
    if (step == GRADIENT_LENGTH - 1) {
      finished = true;
      color = GRADIENT_SEQUENCE[step];
      return;
    }

    const Color currStepColor = GRADIENT_SEQUENCE[step];
    const Color nextStepColor = GRADIENT_SEQUENCE[step + 1];
    const int newRed = currStepColor.red + (nextStepColor.red - currStepColor.red) * 1. * iteration / GRADIENT_STEP_ITERATIONS;
    const int newGreen = currStepColor.green + (nextStepColor.green - currStepColor.green) * 1. * iteration / GRADIENT_STEP_ITERATIONS;
    const int newBlue = currStepColor.blue + (nextStepColor.blue - currStepColor.blue) * 1. * iteration / GRADIENT_STEP_ITERATIONS;
    color = {newRed, newGreen, newBlue};
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

  int step;

  public:

  LcdView() {
    step = 0;
  }

  void update(const Color &color) {
    step++;
    if (step == LCD_UPDATE_DELAY_STEPS) {
      lcd.setCursor(0, 0);
      lcd.print("                    ");
      lcd.setCursor(0, 0);
      lcd.print(color.red);
      lcd.print(",");
      lcd.print(color.green);
      lcd.print(",");
      lcd.print(color.blue);
      lcd.setCursor(0, 2);
      lcd.print(time.gettime("H:i:s"));
      lcd.setCursor(0, 3);
      if (irrecv.decode(&results)){
            lcd.print(results.value, HEX);
            irrecv.resume();
      }
      step = 0;
    }
  }
};




class Controller {

  private:

  LightModel lightModel;
  LightView lightView;
  LcdView lcdView;
  
  public:

  Controller(const LightModel &lightModel, const LightView &lightView, const LcdView &lcdView) {
    this->lightModel = lightModel;
    this->lightView = lightView;
    this->lcdView = lcdView;
  }

  void execute() {
    if (lightModel.isFinished()) {
      lightModel.reset();
    } else 
    if (!lightModel.isStarted()) {
      lightModel.start();
    } else {
      lightModel.update();
    }
    lightView.update(lightModel.getColor());
    lcdView.update(lightModel.getColor());
  }
};




LightModel lightModel;
LightView lightView;
LcdView lcdView;
Controller controller(lightModel, lightView, lcdView);

void setup(){
  lcd.init();                     
  lcd.backlight();
  time.begin();
  irrecv.enableIRIn();
  irrecv.blink13(true);
//  time.settime(0, 13, 21, 13, 11, 21, 7); 
}

void loop(){
  controller.execute();
  delay(DELAY_LENGTH);
}
