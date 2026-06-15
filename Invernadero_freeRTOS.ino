#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino_FreeRTOS.h>
#include <queue.h>

#define PIN_LM35 A0
#define PIN_HUMEDAD A1
#define PIN_RELAY 8

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int SENSOR_SECO = 800;
const int SENSOR_HUMEDO = 350;

const int HUMEDAD_MINIMA = 35;
const int HUMEDAD_MAXIMA = 60;

const bool RELAY_ACTIVO_LOW = false;

// =====================================================
// ESTRUCTURA PARA DATOS DE SENSORES
// =====================================================
struct DatosSensor {
  int temperaturaX10;
  int humedadRaw;
  int humedadPorcentaje;
};

// =====================================================
// COLA DE COMUNICACION
// =====================================================
QueueHandle_t colaDatos;

bool bombaEncendida = false;

// =====================================================
// FUNCIONES DEL RELAY
// =====================================================
void encenderBomba() {
  if (RELAY_ACTIVO_LOW) {
    digitalWrite(PIN_RELAY, LOW);
  } else {
    digitalWrite(PIN_RELAY, HIGH);
  }

  bombaEncendida = true;
}

void apagarBomba() {
  if (RELAY_ACTIVO_LOW) {
    digitalWrite(PIN_RELAY, HIGH);
  } else {
    digitalWrite(PIN_RELAY, LOW);
  }

  bombaEncendida = false;
}

// =====================================================
// TAREA 1: LECTURA DE SENSORES
// =====================================================
void tareaLectura(void *pvParameters) {
  DatosSensor datos;

  while (true) {
    int lecturaLM35 = analogRead(PIN_LM35);
    int lecturaHumedad = analogRead(PIN_HUMEDAD);

    float voltaje = lecturaLM35 * (5.0 / 1023.0);
    float temperatura = voltaje * 100.0;

    datos.temperaturaX10 = temperatura * 10;
    datos.humedadRaw = lecturaHumedad;

    datos.humedadPorcentaje = map(datos.humedadRaw, SENSOR_SECO, SENSOR_HUMEDO, 0, 100);
    datos.humedadPorcentaje = constrain(datos.humedadPorcentaje, 0, 100);

    xQueueOverwrite(colaDatos, &datos);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// =====================================================
// TAREA 2: CONTROL DE BOMBA
// =====================================================
void tareaControl(void *pvParameters) {
  DatosSensor datos;

  while (true) {
    if (xQueuePeek(colaDatos, &datos, 0) == pdPASS) {

      if (datos.humedadPorcentaje <= HUMEDAD_MINIMA && bombaEncendida == false) {
        encenderBomba();
      }

      if (datos.humedadPorcentaje >= HUMEDAD_MAXIMA && bombaEncendida == true) {
        apagarBomba();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// =====================================================
// TAREA 3: COMUNICACION LCD Y SERIAL
// =====================================================
void tareaComunicacion(void *pvParameters) {
  DatosSensor datos;

  while (true) {
    if (xQueuePeek(colaDatos, &datos, 0) == pdPASS) {

      int tempEntera = datos.temperaturaX10 / 10;
      int tempDecimal = datos.temperaturaX10 % 10;

      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(tempEntera);
      lcd.print(".");
      lcd.print(tempDecimal);
      lcd.print("C ");

      lcd.print("H:");
      lcd.print(datos.humedadPorcentaje);
      lcd.print("%   ");

      lcd.setCursor(0, 1);
      lcd.print("Bomba: ");

      if (bombaEncendida) {
        lcd.print("ON ");
      } else {
        lcd.print("OFF");
      }

      lcd.print("     ");

      Serial.print(F("Temp: "));
      Serial.print(tempEntera);
      Serial.print(F("."));
      Serial.print(tempDecimal);
      Serial.print(F(" C | RAW: "));
      Serial.print(datos.humedadRaw);
      Serial.print(F(" | Humedad: "));
      Serial.print(datos.humedadPorcentaje);
      Serial.print(F("% | Bomba: "));

      if (bombaEncendida) {
        Serial.println(F("ON"));
      } else {
        Serial.println(F("OFF"));
      }

    } else {
      lcd.setCursor(0, 0);
      lcd.print("Esperando datos ");
      lcd.setCursor(0, 1);
      lcd.print("Sensores...     ");

      Serial.println(F("Esperando datos"));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  Serial.begin(9600);

  Serial.println(F("Inicio sistema"));

  pinMode(PIN_RELAY, OUTPUT);
  apagarBomba();

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Sistema Riego");
  lcd.setCursor(0, 1);
  lcd.print("FreeRTOS OK");

  Serial.println(F("LCD iniciada"));

  colaDatos = xQueueCreate(1, sizeof(DatosSensor));

  if (colaDatos == NULL) {
    Serial.println(F("Error cola"));

    lcd.setCursor(0, 0);
    lcd.print("Error cola      ");
    lcd.setCursor(0, 1);
    lcd.print("Revise memoria  ");

    while (1);
  }

  Serial.println(F("Cola creada"));

  if (xTaskCreate(tareaLectura, "Lectura", 128, NULL, 3, NULL) == pdPASS) {
    Serial.println(F("Tarea lectura OK"));
  } else {
    Serial.println(F("Error lectura"));
  }

  if (xTaskCreate(tareaControl, "Control", 128, NULL, 2, NULL) == pdPASS) {
    Serial.println(F("Tarea control OK"));
  } else {
    Serial.println(F("Error control"));
  }

  if (xTaskCreate(tareaComunicacion, "Comunic", 160, NULL, 1, NULL) == pdPASS) {
    Serial.println(F("Tarea comunic OK"));
  } else {
    Serial.println(F("Error comunic"));
  }

  Serial.println(F("Sistema funcionando"));
}

void loop() {
}