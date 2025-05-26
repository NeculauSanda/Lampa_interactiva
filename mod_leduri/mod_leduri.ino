// Neculau Sanda-Elena 334CB

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>

// initializare lcd si dfplayer
LiquidCrystal_I2C lcd(0x27, 16, 2);
DFRobotDFPlayerMini player;

// initializare pini led-uri rgb
const int led1R = 6, led1G = 5, led1B = 3;
const int led2R = 9, led2G = 10, led2B = 11;
const int ldrPin = A0; // pin analog fotorezistor
const int touchPin = 2; // pin senzor touch

volatile bool touchDetected = false;
// modul de luminare
volatile int mode = 1;

unsigned long lastTouchTime = 0;
unsigned long firstTouchTime = 0;
int touchCount = 0;
bool waitingForDecision = false;

// volumul initial al difuzorului
int volum = 10;

void setup() {
  // setare pini rgb pentru iesire
  int pins[] = {led1R, led1G, led1B, led2R, led2G, led2B};
  for (int i = 0; i < 6; i++) pinMode(pins[i], OUTPUT);

  // pinul touch(2) ca input
  pinMode(touchPin, INPUT);
  // Seteaza o intrerupere hardware pe pinul conectat la senzorul de atingere (touchPin), 
  // care va executa automat functia schimbareMod() atunci cand semnalul de pe pin trece
  // de la LOW la HIGH (adica o atingere pe senzor).
  attachInterrupt(digitalPinToInterrupt(touchPin), schimbareMod, RISING);

  // initializare comunicati seriale
  Serial.begin(9600);
  Serial1.begin(9600); //dfplayer
  // pornire lcd
  lcd.begin();
  lcd.backlight();

  if (!player.begin(Serial1)) {
    Serial.println("DFPlayer nu a fost gasit!");
    while (true);
  }

  // setam pe dfplayer volumul si melodia
  player.volume(volum);
  player.play(mode);

  // afisam pe lcd
  lcd.setCursor(0, 0); lcd.print("Mod: ");
  lcd.setCursor(0, 1); lcd.print("Volum: ");
  afiseazaMod();
  afiseazaVolum();
}

void loop() {
  unsigned long currentMillis = millis();

  // Detectie atingeri SENZOR
  if (touchDetected && currentMillis - lastTouchTime > 200) {
    touchDetected = false;
    lastTouchTime = currentMillis;

    if (!waitingForDecision) {
      //firstTouchTime primeste timpul actual la care s-a facut apasarea
      firstTouchTime = currentMillis;
      touchCount = 1;
      // s-a detectat prima atingere se astepata si dupa urmatoarele in actiuni
      //(daca se mai apasa sau nu butonul)
      waitingForDecision = true;
    } else {
      touchCount++;
    }
  }

  // Dupa 1,5 secunde: executa actiunea (SCHIMBARE MOD, VOLUM UP, VOLUM DOWN)
  if (waitingForDecision && currentMillis - firstTouchTime > 1500) {
    if (touchCount == 1) {
      mode++;
      if (mode > 6) mode = 1;
      Serial.print("Mod schimbat la: "); Serial.println(mode);
      player.play(mode);
      afiseazaMod();
    } else if (touchCount == 2) {
      volum = min(volum + 2, 26);
      player.volume(volum);
      Serial.print("Volum marit: "); Serial.println(volum);
      afiseazaVolum();
    } else if (touchCount >= 3) {
      volum = max(volum - 2, 0);
      player.volume(volum);
      Serial.print("Volum scazut: "); Serial.println(volum);
      afiseazaVolum();
    }

    // Resetam numarul de apasari si daca s-a facut actiunea
    touchCount = 0;
    waitingForDecision = false;
  }

  // Daca piesa s-a terminat, o reluam
  if (player.available()) {
    if (player.readType() == DFPlayerPlayFinished) {
      Serial.println("Piesa terminata. Reluam...");
      player.play(mode);
    }
  }

  // Moduri LED
  switch (mode) {
    case 1: staticColors(); break;
    case 2: fadingColors(); break;
    case 3: blinkRGB(); break;
    case 4: fadeWhiteBlue(); break;
    case 5: rainbowFade(); break;
    case 6: pingPong(); break;
  }
}

void schimbareMod() {
  touchDetected = true;
}

void afiseazaMod() {
  lcd.setCursor(5, 0);
  lcd.print("  ");
  lcd.setCursor(5, 0);
  lcd.print(mode);
}

void afiseazaVolum() {
  lcd.setCursor(8, 1);
  if (volum < 10) lcd.print(" ");
  lcd.print(volum);
  lcd.print("  ");
}

// Mod care reactioneaza la lumina ambientala din camera(fotorezistor)
//1. SCHIMBARE ROSU-VERDE-ALBASTRU
void staticColors() {
  static unsigned long lastUpdate = 0;
  static int colorIndex = 0;
  const unsigned long delayTime = 500;
  if (millis() - lastUpdate >= delayTime) {
    lastUpdate = millis();

    // citire analog fotorezistor A0 si aifsare valoare ambientala
    int lumina = analogRead(ldrPin);
    Serial.print("Luminozitate ");
    Serial.println(lumina);
    delay(500);

    if (lumina < 250) {
      switch (colorIndex) {
        case 0: setAll(255, 0, 0); break;
        case 1: setAll(0, 255, 0); break;
        case 2: setAll(0, 0, 255); break;
      }
      colorIndex = (colorIndex + 1) % 3;
    } else {
      setAll(0, 0, 0);
    }
  }
}

//2. Trece prin culori rainbow 
void fadingColors() {
  static int phase = 0, i = 0;
  static unsigned long lastUpdate = 0;
  const int stepDelay = 5;
  if (millis() - lastUpdate < stepDelay) return;
  lastUpdate = millis();
  switch (phase) {
    case 0: setAll(i, 0, 0); i++; if (i > 255) { i = 255; phase = 1; } break;
    case 1: setAll(i, 255 - i, 0); i--; if (i < 0) { i = 0; phase = 2; } break;
    case 2: setAll(0, 255 - i, i); i++; if (i > 255) { i = 0; phase = 0; } break;
  }
}

// 3. Clipeste rapid in culoarea alb
void blinkRGB() {
  static unsigned long lastUpdate = 0;
  static bool on = false;
  const unsigned long delayTime = 200;
  if (millis() - lastUpdate >= delayTime) {
    setAll(on ? 255 : 0, on ? 255 : 0, on ? 255 : 0);
    on = !on;
    lastUpdate = millis();
  }
}

// 4. mod relaxant- trecere lina(scadere intensitate) de la alb la albastru
void fadeWhiteBlue() {
  static int brightness = 0, dir = 1;
  static unsigned long lastUpdate = 0;
  const unsigned long stepDelay = 10;
  if (millis() - lastUpdate >= stepDelay) {
    setAll(brightness, brightness, 255);
    brightness += dir;
    if (brightness >= 125 || brightness <= 0) dir *= -1;
    lastUpdate = millis();
  }
}

// 5. Mod rainbow diferit de 2
void rainbowFade() {
  static unsigned long lastUpdate = 0;
  const unsigned long stepDelay = 5;
  if (millis() - lastUpdate >= stepDelay) {
    static int red = 254, green = 1, blue = 127;
    static int red_dir = -1, green_dir = 1, blue_dir = -1;
    red += red_dir; green += green_dir; blue += blue_dir;
    if (red >= 255 || red <= 0) red_dir *= -1;
    if (green >= 255 || green <= 0) green_dir *= -1;
    if (blue >= 255 || blue <= 0) blue_dir *= -1;
    setAll(red, green, blue);
    lastUpdate = millis();
  }
}

// 6. mod pingpong - alternare lumina de la un led la altu, culoare fuxia
void pingPong() {
  static unsigned long lastUpdate = 0;
  static bool onFirst = true;
  const unsigned long delayTime = 300;
  if (millis() - lastUpdate >= delayTime) {
    if (onFirst) {
      analogWrite(led1R, 155); analogWrite(led1G, 0); analogWrite(led1B, 100);
      analogWrite(led2R, 0); analogWrite(led2G, 0); analogWrite(led2B, 0);
    } else {
      analogWrite(led1R, 0); analogWrite(led1G, 0); analogWrite(led1B, 0);
      analogWrite(led2R, 155); analogWrite(led2G, 0); analogWrite(led2B, 100);
    }
    onFirst = !onFirst;
    lastUpdate = millis();
  }
}

// seteaza led-uri pe aceleasi culori
void setAll(int r, int g, int b) {
  analogWrite(led1R, r); analogWrite(led1G, g); analogWrite(led1B, b);
  analogWrite(led2R, r); analogWrite(led2G, g); analogWrite(led2B, b);
}
