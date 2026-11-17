#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ThingSpeak.h>

// ================= CONFIGURACIÓN DE CREDENCIALES =================
const char* WIFI_SSID = "Portal_IS-IR";
const char* WIFI_PASS = "S4telit3";

const unsigned long SECRET_CH_ID = 3543378;
const char* SECRET_WRITE_APIKEY = "GQ74YWTHQR33J57K";
// =================================================================

// ================= DEFINICIÓN DE PINES Y HARDWARE ================
#define PIN_DHT    14    // D5 (GPIO 14) - Sensor de Temperatura y Humedad DHT11
#define PIN_LDR    A0    // A0 (ADC0)    - 2do Sensor: Fotorresistencia LDR (Luz ambiental en pista)
#define PIN_BUZZER 12    // D6 (GPIO 12) - 2do Actuador: Buzzer sonoro de alerta
#define DHTTYPE    DHT11

// Pines del bloque LED de pista (incluye pines de esquina por compatibilidad física)
const int PINES_LED[] = {1, 3, 16, 5, 4, 0};
const int CANT_PINES_LED = 6;

// ================= CONSTANTES Y UMBRALES OPERATIVOS ==============
// Cambiá UMBRAL_TEMP a 20.0 para grabar el Punto 3 (Video en estado de ALERTA)
// Dejalo en 32.0 para estado NORMAL (LED fijo)
const float UMBRAL_TEMP = 32.0;          
const int UMBRAL_LUZ_BAJA = 300;         // Umbral analógico (0-1023) para detección de visibilidad en pista

const unsigned long INTERVALO_LECTURA    = 2000;  // 2 s entre lecturas de sensores
const unsigned long INTERVALO_THINGSPEAK = 15000; // 15 s entre envíos a la nube
const unsigned long INTERVALO_ALARMA     = 250;   // 250 ms para baliza estroboscópica y sirena

// ================= OBJETOS Y VARIABLES GLOBALES ==================
DHT dht(PIN_DHT, DHTTYPE);
WiFiClient client;

unsigned long ultimoTiempoLectura    = 0;
unsigned long ultimoTiempoThingSpeak = 0;
unsigned long ultimoTiempoAlarma     = 0;

float temperaturaActual = 25.0;
float humedadActual     = 60.0;
int nivelLuzActual      = 500;

bool alarmaTermicaActiva = false;
bool bajaVisibilidad     = false;
bool estadoPulsoAlerta   = false;

// ================= PROTOTIPOS DE FUNCIONES AUXILIARES ============
void inicializarHardware();
void inicializarComunicaciones();
void establecerEstadoLED(int estado);
void leerSensores();
void evaluarEstadoSistema();
void gestionarActuadores(unsigned long tiempoActual);
void enviarTelemetriaThingSpeak();

// ================= FUNCIÓN PRINCIPAL DE ARRANQUE =================
void setup() {
  inicializarHardware();
  inicializarComunicaciones();
}

// ================= BUCLE PRINCIPAL NO BLOQUEANTE =================
void loop() {
  unsigned long tiempoActual = millis();

  // 1. Tarea periódica: Lectura de sensores y evaluación de umbrales
  if (tiempoActual - ultimoTiempoLectura >= INTERVALO_LECTURA) {
    ultimoTiempoLectura = tiempoActual;
    leerSensores();
    evaluarEstadoSistema();
  }

  // 2. Tarea continua: Control no bloqueante de actuadores (LED de pista + Buzzer)
  gestionarActuadores(tiempoActual);

  // 3. Tarea periódica: Transmisión de telemetría a ThingSpeak
  if (tiempoActual - ultimoTiempoThingSpeak >= INTERVALO_THINGSPEAK) {
    ultimoTiempoThingSpeak = tiempoActual;
    enviarTelemetriaThingSpeak();
  }
}

// ================= IMPLEMENTACIÓN DE FUNCIONES ===================

void inicializarHardware() {
  // Habilitar pines TX/RX como GPIO estándar para el LED externo de la maqueta
  pinMode(1, FUNCTION_3);
  pinMode(3, FUNCTION_3);

  for (int i = 0; i < CANT_PINES_LED; i++) {
    pinMode(PINES_LED[i], OUTPUT);
  }

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Estado inicial: Pista iluminada y buzzer apagado
  establecerEstadoLED(HIGH);
  dht.begin();
}

void inicializarComunicaciones() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ThingSpeak.begin(client);
}

void establecerEstadoLED(int estado) {
  for (int i = 0; i < CANT_PINES_LED; i++) {
    digitalWrite(PINES_LED[i], estado);
  }
}

void leerSensores() {
  // Lectura del Sensor 1: DHT11 (Temperatura del motor y Humedad relativa)
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temperaturaActual = t;
    humedadActual = h;
  }

  // Lectura del Sensor 2: LDR en A0 (Luminosidad / Visibilidad en pista)
  nivelLuzActual = analogRead(PIN_LDR);
}

void evaluarEstadoSistema() {
  alarmaTermicaActiva = (temperaturaActual > UMBRAL_TEMP);
  bajaVisibilidad     = (nivelLuzActual < UMBRAL_LUZ_BAJA);
}

void gestionarActuadores(unsigned long tiempoActual) {
  if (alarmaTermicaActiva) {
    // MODO ALERTA: Parpadeo rápido del LED de pista + pitido intermitente del Buzzer con millis()
    if (tiempoActual - ultimoTiempoAlarma >= INTERVALO_ALARMA) {
      ultimoTiempoAlarma = tiempoActual;
      estadoPulsoAlerta = !estadoPulsoAlerta;

      establecerEstadoLED(estadoPulsoAlerta ? HIGH : LOW);
      digitalWrite(PIN_BUZZER, estadoPulsoAlerta ? HIGH : LOW);
    }
  } else {
    // MODO NORMAL: LED de pista encendido fijo (operación segura) y Buzzer en silencio
    establecerEstadoLED(HIGH);
    digitalWrite(PIN_BUZZER, LOW);
  }
}

void enviarTelemetriaThingSpeak() {
  if (WiFi.status() == WL_CONNECTED) {
    ThingSpeak.setField(1, temperaturaActual);
    ThingSpeak.setField(2, humedadActual);
    ThingSpeak.setField(3, nivelLuzActual); // Campo 3 para el nuevo sensor LDR
    ThingSpeak.writeFields(SECRET_CH_ID, SECRET_WRITE_APIKEY);
  }
}
