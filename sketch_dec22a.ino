#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Configuración LCD (I2C) en pines 4 y 5
#define SDA_PIN 4
#define SCL_PIN 5
LiquidCrystal_I2C lcd(0x27, 16, 2); // Dirección I2C: 0x27, pantalla de 16 columnas y 2 filas

// Pines del ESP32
const int relayPin = 23;       // Relay conectado a GPIO23
const int buttonPin = 18;      // Botón conectado a GPIO18
const int pirInsidePin = 19;   // Sensor PIR interno conectado a GPIO19
const int pirOutsidePin = 21;  // Sensor PIR externo conectado a GPIO21
const int playPin = 22;        // ISD1820 (PLAYE) conectado a GPIO22

// Variables del sistema
enum State { WAITING, READY_TO_EXIT, DETECTED_INSIDE, DETECTED_OUTSIDE, OUTSIDE, ENTERING, INSIDE };
State systemState = WAITING;   // Estado inicial del sistema
bool doorAlwaysOpen = false;   // Modo "puerta abierta"
bool insideDetected = false;   // Detección del sensor interno
bool outsideDetected = false;  // Detección del sensor externo
unsigned long lastCycleTime = 0;   // Tiempo del último ciclo
const unsigned long cycleDuration = 60000; // Duración del ciclo (1 minuto en milisegundos)
bool systemActive = false;  // Sistema inicia desactivado

void setup() {
  // Configuración inicial
  Serial.begin(115200);
  
  // Configurar pines personalizados para SDA y SCL
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100); // Dar tiempo al I2C para estabilizarse

  // Verificar si la LCD está conectada
  Wire.beginTransmission(0x27);
  if (Wire.endTransmission() != 0) {
    Serial.println("No se encontró la LCD en 0x27");
    // Intentar con dirección alternativa
    Wire.beginTransmission(0x3F);
    if (Wire.endTransmission() != 0) {
      Serial.println("No se encontró la LCD en 0x3F");
      while(1); // Detener si no se encuentra la LCD
    }
  }

  pinMode(relayPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(pirInsidePin, INPUT);
  pinMode(pirOutsidePin, INPUT);
  pinMode(playPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // Puerta cerrada inicialmente
  digitalWrite(playPin, LOW);   // ISD1820 apagado

  // Inicializar la pantalla LCD
  lcd.begin(16, 2);  // Inicializar antes del init
  delay(50);
  lcd.init();
  delay(50);
  lcd.backlight();
  delay(50);
  
  // Configurar el LCD
  lcd.clear();
  lcd.home();
  lcdPrint("Sistema", "Desactivado");
  delay(1000);
}

void loop() {
  // Verificar botón
  if (digitalRead(buttonPin) == LOW) {
    handleButtonPress();
    delay(300);
    return;
  }

  // Sistema desactivado
  if (!systemActive && !doorAlwaysOpen) {
    lcdPrint("Sistema", "Desactivado");
    digitalWrite(relayPin, LOW);
    return;
  }

  // Modo puerta siempre abierta (triple click)
  if (doorAlwaysOpen) {
    digitalWrite(relayPin, HIGH);
    lcdPrint("Modo Puerta Abierta", "Relay Activo");
    return;
  }

  // Lógica principal del sistema
  switch (systemState) {
    case WAITING:
      lcdPrint("Esperando", "1 minuto");
      digitalWrite(relayPin, LOW);
      if (millis() - lastCycleTime >= cycleDuration) {
        digitalWrite(relayPin, HIGH);
        systemState = OUTSIDE;
        lcdPrint("Puerta Abierta", "Esperando perro");
      }
      break;

    case OUTSIDE:
      // Mantener el relé activo indefinidamente
      digitalWrite(relayPin, HIGH);
      
      // Solo cambiar de estado si detecta movimiento en el sensor interno
      if (digitalRead(pirInsidePin) == HIGH) {
        lcdPrint("Perro dentro", "Cerrando puerta");
        delay(2000);
        closeDoor();
        systemState = WAITING;
        lastCycleTime = millis();  // Reiniciar el temporizador
      }
      break;
  }
}

// Función para manejar las presiones del botón
void handleButtonPress() {
  static int buttonPressCount = 0;
  static unsigned long lastButtonPress = 0;
  unsigned long currentTime = millis();

  // Verificar si es una secuencia rápida
  if (currentTime - lastButtonPress <= 500) {
    buttonPressCount++;
  } else {
    buttonPressCount = 1;
  }
  
  lastButtonPress = currentTime;

  // Modo puerta abierta con triple click (sin cambios)
  if (buttonPressCount == 3) {
    doorAlwaysOpen = !doorAlwaysOpen;
    buttonPressCount = 0;
    if (doorAlwaysOpen) {
      systemActive = true;  // Activar el sistema si se activa modo puerta abierta
      Serial.println("Modo Puerta Abierta Activado");
      lcdPrint("Modo Puerta Abierta", "Relay Activo");
      digitalWrite(relayPin, HIGH);
    } else {
      Serial.println("Modo Puerta Abierta Desactivado");
      lcdPrint("Modo Puerta Abierta", "Relay Inactivo");
      digitalWrite(relayPin, LOW);
    }
    return;
  }

  // Click simple ahora activa/desactiva el sistema
  if (buttonPressCount == 1) {
    systemActive = !systemActive;
    if (systemActive) {
      digitalWrite(relayPin, HIGH);
      lcdPrint("Sistema Activado", "Monitoreando...");
    } else {
      digitalWrite(relayPin, LOW);
      lcdPrint("Sistema", "Desactivado");
    }
  }
}

// Función para abrir la puerta
void openDoor() {
  digitalWrite(relayPin, HIGH);
  Serial.println("Puerta abierta.");
  playSound();
}

// Función para cerrar la puerta
void closeDoor() {
  digitalWrite(relayPin, LOW);
  Serial.println("Puerta cerrada.");
  playSound();
}

// Función para reproducir sonido
void playSound() {
  digitalWrite(playPin, HIGH);
  delay(1000);
  digitalWrite(playPin, LOW);
}

// Función para imprimir en la LCD
void lcdPrint(String line1, String line2) {
  lcd.clear();
  delay(5);
  lcd.home();
  delay(5);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  delay(5);
  lcd.print(line2);
}
