#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ThingSpeak.h>

// ================= CONFIGURACIÓN DE USUARIO =================
const char* WIFI_SSID = "Portal_IS-IR";
const char* WIFI_PASS = "S4telit3";

unsigned long SECRET_CH_ID = 3543378;                  
const char* SECRET_WRITE_APIKEY = "GQ74YWTHQR33J57K";  
// ============================================================

// Definición fija de pines según hardware armado
#define PIN_DHT    14  // D5 (GPIO 14) - Cable amarillo DHT11
#define PIN_LED    5   // D1 (GPIO 5)  - LED Rojo de Alerta
#define PIN_BUZZER 0   // D3 (GPIO 0)  - Buzzer (cambiar a 12 si usan D6)
#define DHTTYPE    DHT11

const float UMBRAL_TEMP = 35.0; // °C (bajar a 30.0 si quieren disparar alerta más fácil en el video)

// Intervalos de temporización no bloqueante (millis)
const unsigned long INTERVALO_LECTURA    = 2000;   // 2 segundos
const unsigned long INTERVALO_THINGSPEAK = 15000;  // 15 segundos
const unsigned long INTERVALO_ALARMA     = 300;    // 300 ms
const unsigned long INTERVALO_WIFI       = 10000;  // 10 segundos

DHT dht(PIN_DHT, DHTTYPE);
WiFiClient client;

unsigned long ultimoTiempoLectura    = 0;
unsigned long ultimoTiempoThingSpeak = 0;
unsigned long ultimoTiempoAlarma     = 0;
unsigned long ultimoTiempoWiFi       = 0;

float temperaturaActual = 0.0;
float humedadActual     = 0.0;
bool alarmaActiva       = false;
bool estadoPulsoAlarma  = false;

// Prototipos de funciones auxiliares
void inicializarHardware();
void gestionarWiFi(unsigned long tiempoActual);
void leerSensores(unsigned long tiempoActual);
void evaluarAlertaTermica();
void controlarActuadores(unsigned long tiempoActual);
void enviarTelemetriaThingSpeak(unsigned long tiempoActual);

void setup() {
  Serial.begin(115200);
  inicializarHardware();
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ThingSpeak.begin(client);
  Serial.println(F("\n[SISTEMA] AgroFly Thermal Guardian iniciado (Modo No Bloqueante)."));
}

void loop() {
  unsigned long tiempoActual = millis();

  gestionarWiFi(tiempoActual);
  leerSensores(tiempoActual);
  evaluarAlertaTermica();
  controlarActuadores(tiempoActual);
  enviarTelemetriaThingSpeak(tiempoActual);
}

// ================= FUNCIONES AUXILIARES =================

void inicializarHardware() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  noTone(PIN_BUZZER);
  digitalWrite(PIN_BUZZER, LOW);
  dht.begin();
}

void gestionarWiFi(unsigned long tiempoActual) {
  if (WiFi.status() == WL_CONNECTED) return;

  if (tiempoActual - ultimoTiempoWiFi >= INTERVALO_WIFI) {
    ultimoTiempoWiFi = tiempoActual;
    Serial.println(F("[WIFI] Reintentando conexion..."));
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}

void leerSensores(unsigned long tiempoActual) {
  if (tiempoActual - ultimoTiempoLectura >= INTERVALO_LECTURA) {
    ultimoTiempoLectura = tiempoActual;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
      Serial.println(F("[AVISO] Error de lectura en DHT11 (verificar cable D5)."));
      return;
    }

    temperaturaActual = t;
    humedadActual     = h;

    Serial.printf("[SENSOR] Temp: %.1f C | Hum: %.1f %% | Alerta: %s | WiFi: %s\n",
                  temperaturaActual, humedadActual,
                  alarmaActiva ? "SI" : "NO",
                  (WiFi.status() == WL_CONNECTED) ? "CONECTADO" : "BUSCANDO...");
  }
}

void evaluarAlertaTermica() {
  alarmaActiva = (temperaturaActual > UMBRAL_TEMP);
}

void controlarActuadores(unsigned long tiempoActual) {
  if (!alarmaActiva) {
    estadoPulsoAlarma = false;
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
    digitalWrite(PIN_BUZZER, LOW);
    return;
  }

  if (tiempoActual - ultimoTiempoAlarma >= INTERVALO_ALARMA) {
    ultimoTiempoAlarma = tiempoActual;
    estadoPulsoAlarma = !estadoPulsoAlarma;

    if (estadoPulsoAlarma) {
      digitalWrite(PIN_LED, HIGH);
      tone(PIN_BUZZER, 2500);
    } else {
      digitalWrite(PIN_LED, LOW);
      noTone(PIN_BUZZER);
    }
  }
}

void enviarTelemetriaThingSpeak(unsigned long tiempoActual) {
  if (tiempoActual - ultimoTiempoThingSpeak >= INTERVALO_THINGSPEAK) {
    ultimoTiempoThingSpeak = tiempoActual;

    if (WiFi.status() == WL_CONNECTED && temperaturaActual != 0.0) {
      ThingSpeak.setField(1, temperaturaActual);
      ThingSpeak.setField(2, humedadActual);
      ThingSpeak.setField(3, alarmaActiva ? 1 : 0);

      int codigoHTTP = ThingSpeak.writeFields(SECRET_CH_ID, SECRET_WRITE_APIKEY);
      if (codigoHTTP == 200) {
        Serial.println(F("[THINGSPEAK] Datos enviados correctamente (HTTP 200)."));
      } else {
        Serial.println("[THINGSPEAK] Error HTTP: " + String(codigoHTTP));
      }
    }
  }
}
