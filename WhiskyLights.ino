#include <FastLED.h>

#include <IRremote.h>
#include <IRremoteInt.h>

// LEDs
#define NUM_LEDS 80
#define DATA_PIN 13
#define CLOCK_PIN 11

// Remote control
#define IRREC_PIN 3

#define IR_POWER 0xE318261B
#define IR_VOLUP 0x511DBB
#define IR_FUNC  0xEE886D7F
#define IR_FBACK 0x52A3D41F
#define IR_PLAY  0xD7E84B1B
#define IR_FFORW 0x20FE4DBB
#define IR_DOWN  0xAC7A496F
#define IR_VOLDN 0xA3C8EDDB
#define IR_UP    0xE5CFBD7F
#define IR_EQ    0xFF9867
#define IR_REPT  0xF0C41643
#define IR_ZERO  0xC101E57B
#define IR_ONE   0x9716BE3F
#define IR_TWO   0xFF18E7
#define IR_THREE 0xFF7A85
#define IR_FOUR  0xFF10EF
#define IR_FIVE  0x488F3CBB
#define IR_SIX   0x449E79F
#define IR_SEVEN 0x32C6FDF7
#define IR_EIGHT 0x1BC0157B
#define IR_NINE  0xFF52AD

// Audio analysis
#define STROBEPIN 4
#define RESET     5
#define DC_One    A0
#define DC_Two    A1

#define BASE_NOISE 70
#define SPEC_TRAIL 25

// Effects
#define MAX_FADE_SPEED 10
#define MAX_BRIGHTNESS 10
#define HUE_OFFSET     120

enum lightstate {
  OFF,
  REC_COMMAND,
  EQ_VIS,
  FIX_COLOR,
  FADE,
  STROBE,
  FREEZE
};

CRGB leds[NUM_LEDS];

IRrecv irrecv(IRREC_PIN);
decode_results results;

//Define spectrum variables
int freq_amp;
int raw_spectrum[7][SPEC_TRAIL];
int rs_ptr = 0;
int spectrum[7];

lightstate state = FIX_COLOR;
lightstate last_state = FIX_COLOR;
unsigned int fade_speed = 1;
unsigned int brightness = MAX_BRIGHTNESS;

CHSV current_color = CHSV(0, 0, 255);

void setup() {
  FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, NUM_LEDS);

  irrecv.enableIRIn(); // Start the receiver

  //Set spectrum Shield pin configurations
  pinMode(STROBEPIN, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(DC_One, INPUT);
  pinMode(DC_Two, INPUT);  
  digitalWrite(STROBEPIN, HIGH);
  digitalWrite(RESET, HIGH);
  
  //Initialize Spectrum Analyzers
  digitalWrite(STROBEPIN, LOW);
  delay(1);
  digitalWrite(RESET, HIGH);
  delay(1);
  digitalWrite(STROBEPIN, HIGH);
  delay(1);
  digitalWrite(STROBEPIN, LOW);
  delay(1);
  digitalWrite(RESET, LOW);

  for (int i = 0; i < 7; ++i){
    for (int j = 0; j < SPEC_TRAIL; ++j) {
      raw_spectrum[i][j] = 0; 
    }
  }
}

void loop() {  
  UpdateIR();

  switch (state) {
    case OFF: 
      SetAll(CHSV(0, 0, 0));
      break;
    case FREEZE:
      break;
    case FIX_COLOR:
      SetAll(current_color);
      break;
    case FADE:
      current_color.h += 1;
      delay(5 + (10 - fade_speed) * 7);
      SetAll(current_color);
      break;
    case EQ_VIS:
      Read_Frequencies();
      Refine();
      Visualize();
      break;
    case STROBE:
      delay(5 + (10 - fade_speed) * 15);
      SetAll(current_color); 
      FastLED.show();     
      delay(5 + (10 - fade_speed) * 15);
      SetAll(CHSV(0,0,0));
  }

  FastLED.show(); // Update LEDs
}

void Refine() {
  int freq_avg = 0;
  for (int i = 0; i < 7; ++i){   
    freq_avg = 0; 
    for (int j = 0; j < SPEC_TRAIL; ++j) {
      freq_avg += raw_spectrum[i][j];
    }
    spectrum[i] = freq_avg / SPEC_TRAIL;
  }
  
  int max_value = 0;
  for (int i = 0; i < 7; ++i) {
    spectrum[i] = max(spectrum[i] - BASE_NOISE, 0);
    max_value = max(spectrum[i], max_value);
  }

  for (int i = 0; i < 7; ++i) {
    spectrum[i] = (int) (((float) spectrum[i] / max_value) * 255.0);
    spectrum[i] += HUE_OFFSET;
    spectrum[i] %= 255;
  }
}

void Visualize(){
  for (int i = 0; i < NUM_LEDS; ++i) {
    int band = 7.0 * ((float) i / (float) NUM_LEDS);   
    int _hue = spectrum[band];
    leds[i] = HSVFromHS(_hue, 255);
  }
}

void SetAll(CHSV _color) {  
  for (int i = 0; i < NUM_LEDS; ++i) {
    leds[i] = _color;
  }
}

void UpdateIR() {
  if (!irrecv.decode(&results)) {
    return;  // No update
  }

  float _brightness;
  switch (results.value) {
    case IR_POWER: // Toggle on/off
      last_state = (state == OFF ? last_state : state);
      state = (state == OFF ? last_state : OFF); 
      break;
    case IR_VOLUP: // Brightness up
      brightness = min(brightness + 1, MAX_BRIGHTNESS);
      
      _brightness = (brightness / (float) MAX_BRIGHTNESS) * 255.0;
      current_color.v = (int) _brightness;
      break;
    case IR_VOLDN: // Brightness down
      brightness = max(brightness - 1, 1);
      
      _brightness = (brightness / (float) MAX_BRIGHTNESS) * 255.0;
      current_color.v = (int) _brightness;
      break;
    case IR_FFORW: // Fade speed up
      if (state == FADE) {
        fade_speed = min(fade_speed + 1, MAX_FADE_SPEED);
      }
      break;
    case IR_FBACK: // Fade speed down
      if (state == FADE) {
        fade_speed = max(fade_speed - 1, 1);
      }
      break;
    case IR_UP: // Increase hue  
      if (state == FIX_COLOR) {
        current_color.h = (current_color.h + 5) % 255;         
      }
      break;
    case IR_DOWN: // Decrease hue  
      if (state == FIX_COLOR) {
        current_color.h = (current_color.h - 5) % 255;         
      }
      break;
    case IR_REPT: // Start fade mode
      last_state = state;      
      state = (state == OFF ? OFF : FADE);
      if (state == FADE) {
        current_color = HSVFromHS(25, 255);
      }
      break;
    case IR_PLAY: // Freeze 
      if (state == FADE || state == EQ_VIS || state == STROBE) {
        last_state = state;
        state = FREEZE;
      } else if (state == FREEZE && (last_state == EQ_VIS || last_state == FADE || last_state == STROBE)) {
        state = last_state;
        last_state = FREEZE;
      }
      break;
    case IR_EQ: // Start equalizer display
      last_state = state;      
      state = (state == OFF ? OFF : EQ_VIS);
      break;
    case IR_FUNC: // Strobe
      last_state = state;
      state = (state == OFF ? OFF : STROBE);
      break;
    case IR_ZERO:
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(0, 0);
      break;
    case IR_ONE:   
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(25, 255);
      break;
    case IR_TWO:   
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(50, 255);
      break;
    case IR_THREE: 
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(75, 255);
      break;
    case IR_FOUR:  
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(100, 255);
      break;
    case IR_FIVE:   
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(125, 255);
      break;
    case IR_SIX:   
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(150, 255);
      break;
    case IR_SEVEN: 
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(175, 255);
      break;
    case IR_EIGHT: 
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(200, 255);
      break;
    case IR_NINE:  
      last_state = state;      
      state = (state == OFF ? OFF : FIX_COLOR);
      current_color = HSVFromHS(225, 255);
      break;
    default: // Unknown button
      break;
  }
  
  delay(100);
  irrecv.resume();
}

CHSV HSVFromHS(unsigned int _hue, unsigned int _saturation) {
  float _brightness = (brightness / (float) MAX_BRIGHTNESS) * 255.0;
  
  return CHSV(_hue, _saturation, min(255, (int) _brightness));
}

void Read_Frequencies(){
  //Read frequencies for each band
  for (freq_amp = 0; freq_amp<7; freq_amp++)
  {
    raw_spectrum[freq_amp][rs_ptr] = (analogRead(DC_One) + analogRead(DC_Two)) / 2;
    rs_ptr++;
    rs_ptr %= SPEC_TRAIL;
    
    digitalWrite(STROBEPIN, HIGH);
    digitalWrite(STROBEPIN, LOW);
  }
}
