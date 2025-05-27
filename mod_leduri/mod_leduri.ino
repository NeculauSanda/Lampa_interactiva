// Neculau Sanda-Elena 334CB

#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>
#include <Stream.h> // pentru dfplayer

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

//---------------- Implementare manuala pentru comunicarea seriala Serial0 - USB(debuging) --------------
void manualSerialInit(long baud) {
  // Setare registri pentru UART0 (Serial)
  // calculare viteza de comunicare, F_CPU(viteza procesorului)
  uint16_t ubrr = F_CPU/16/baud-1;
  // impartim valoare pe 2 registri primi 8 biti si restul
  UBRR0H = (unsigned char)(ubrr>>8);
  UBRR0L = (unsigned char)ubrr;
  // Activeaza receptorul (RXEN0) si transmisorul (TXEN0) pentru UART0
  UCSR0B = (1<<RXEN0)|(1<<TXEN0);
  // seteaza marimea datelor transmise la 8 biti 
  UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

void manualSerialWrite(unsigned char data) {
  // Asteaptam ca buffer-ul UDRE0 de transmisie sa se goleasca
  // verifica daca bitul UDRE0 e activ
  while (!(UCSR0A & (1<<UDRE0)));
  // Scrie un caracter in registrul de date pentru al transmite
  UDR0 = data;
}

// trimite caracter cu caracter pana ajunge la final
void manualSerialPrint(const char* str) {
  while (*str) {
    manualSerialWrite(*str++);
  }
}

// transforma numar intreg in caracter ascii si il transmite caracter cu caracter
void manualSerialPrintNumber(int num) {
  if (num == 0) {
    manualSerialWrite('0');
    return;
  }
  
  char buffer[10];
  int i = 0;
  bool negative = false;
  
  if (num < 0) {
    negative = true;
    num = -num;
  }
  
  while (num > 0) {
    // se calculeaza valoarea numarului in caracter ascii
    buffer[i++] = '0' + (num % 10);
    num /= 10;
  }
  
  // daca e negativ adaugam un - in fata
  if (negative) {
    manualSerialWrite('-');
  }
  
  // Afiseaza cifrele in ordine inversa
  for (int j = i - 1; j >= 0; j--) {
    manualSerialWrite(buffer[j]);
  }
}

void manualSerialPrintln(const char* str) {
  manualSerialPrint(str);
  manualSerialWrite('\r');
  manualSerialWrite('\n');
}

// ------------------ Implementare manuala pentru comunicarea seriala Serial1 (DFPlayer) --------------
void manualSerial1Init(long baud) {
  // Setare registri pentru UART1 (Serial1)
  // calculare viteza de comunicare, F_CPU(viteza procesorului)
  uint16_t ubrr = F_CPU/16/baud-1;
  // impartim pe 2 registri valoarea
  UBRR1H = (unsigned char)(ubrr>>8);
  UBRR1L = (unsigned char)ubrr;
  // Activeaza receptorul (RXEN1) si transmisorul (TXEN1) pentru UART0
  UCSR1B = (1<<RXEN1)|(1<<TXEN1);
  // setez marimea datelor la 8 biti
  UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);
}

void manualSerial1Write(unsigned char data) {
  // Asteapta buffer-ul de transmisie UDRE1 sa se goleasca
  while (!(UCSR1A & (1<<UDRE1)));
  // scrie caracterul in registru pentru transmisie pe UART1
  UDR1 = data;
}

unsigned char manualSerial1Read() {
  // Asteapta sa se primeasca date pe uart1 apoi il citeste, RXC1 = activ => date disponibile
  while (!(UCSR1A & (1<<RXC1)));
  return UDR1;
}


bool manualSerial1Available() {
  // Verifica daca exista date primite in buffer-ul UART1
  return (UCSR1A & (1<<RXC1));
}

// Clasa pentru Serial1 in DFPlayer care extinde stream
class ManualSerial : public Stream {
public:
  // porneste comunicare cu viteza dorita
  void begin(long baud) {
    manualSerial1Init(baud);
  }

  // functie pentru a transmite datele byte cu byte
  virtual size_t write(uint8_t data) override {
    manualSerial1Write(data);
    return 1;
  }

  // citeste caracterele daca e disponibil
  virtual int read() override {
    if (manualSerial1Available()) {
      return manualSerial1Read();
    }
    return -1;
  }

  // verifica daca exista date disponibile pentru citire
  virtual int available() override {
    return manualSerial1Available() ? 1 : 0;
  }

  virtual int peek() override {
    // Nu avem buffer pentru peek returnam -1
    return -1;
  }

  using Print::write;  // permite folosirea altor supraincarcari din Print
};

// initializare serial1
ManualSerial manualSerial1;

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

  // initializare comunicatii seriale manual
  manualSerialInit(9600);
  manualSerial1.begin(9600); //dfplayer manual
  
  // pornire lcd
  lcd.begin();
  lcd.backlight();

  if (!player.begin(manualSerial1)) {
    manualSerialPrintln("DFPlayer nu a fost gasit!");
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

  // Detectie atingeri SENZOR detectie atingere in 0,2 secunde
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
      manualSerialPrint("Mod schimbat la: ");
      manualSerialPrintNumber(mode);
      manualSerialWrite('\r');
      manualSerialWrite('\n');
      player.play(mode);
      afiseazaMod();
    } else if (touchCount == 2) {
      volum = min(volum + 2, 26);
      player.volume(volum);
      manualSerialPrint("Volum marit: ");
      manualSerialPrintNumber(volum);
      manualSerialWrite('\r');
      manualSerialWrite('\n');
      afiseazaVolum();
    } else if (touchCount >= 3) {
      volum = max(volum - 2, 0);
      player.volume(volum);
      manualSerialPrint("Volum scazut: ");
      manualSerialPrintNumber(volum);
      manualSerialWrite('\r');
      manualSerialWrite('\n');
      afiseazaVolum();
    }

    // Resetam numarul de apasari si modificam flagul de actiune
    touchCount = 0;
    waitingForDecision = false;
  }

  // Daca piesa s-a terminat, o reluam
  if (player.available()) {
    if (player.readType() == DFPlayerPlayFinished) {
      manualSerialPrintln("Piesa terminata. Reluam...");
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
    manualSerialPrint("Luminozitate ");
    manualSerialPrintNumber(lumina);
    manualSerialWrite('\r');
    manualSerialWrite('\n');
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