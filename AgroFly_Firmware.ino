#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ThingSpeak.h>

// ================= CONFIGURACIÓN DE USUARIO =================
const char* WIFI_SSID = "Portal_IS-IR";
const char* WIFI_PASS = "S4telit3";

unsigned long SECRET_CH_ID = 3543378;                  
const char* SECRET_WRITE_APIKEY = "GQ74YWTHQR33J57K";  
// ============================================================

#define PIN_LED    5   // D1
#define PIN_BUZZER 0   // D3
#define DHTTYPE    DHT11

// Todos los pines posibles donde puede estar la otra punta del cable amarillo (H2)
int pinesPosibles[] = {14, 2, 4, 5, 12, 13, 16, 15, 0}; 
String nombresPines[] = {"D5 (GPIO14)", "D4 (GPIO2)", "D2 (GPIO4)", "D1 (GPIO5)", "D6 (GPIO12)", "D7 (GPIO13)", "D0 (GPIO16)", "D8 (GPIO15)", "D3 (GPIO0)"};
int pinEncontrado = -1;

const float UMBRAL_TEMP = 35.0;
const unsigned long INTERVALO_LECTURA = 2000;      
const unsigned long INTERVALO_THINGSPEAK = 15000;  
const unsigned long INTERVALO_ALARMA = 300;        

WiFiClient client;

unsigned long ultimoTiempoLectura = 0;
unsigned long ultimoTiempoThingSpeak = 0;
unsigned long ultimoTiempoAlarma = 0;

float temperaturaActual = 0.0;
float humedadActual = 0.0;
bool alarmaActiva = false;
bool estadoBuzzer = false;

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("\n--- BUSCANDO EL PIN DEL CABLE AMARILLO (H2) ---");
  
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ThingSpeak.begin(client);
}

void loop() {
  unsigned long tiempoActual = millis();

  if (tiempoActual - ultimoTiempoLectura >= INTERVALO_LECTURA) {
    ultimoTiempoLectura = tiempoActual;
    
    float t = NAN;
    float h = NAN;

    if (pinEncontrado != -1) {
      DHT dht(pinEncontrado, DHTTYPE);
      dht.begin();
      delay(250);
      t = dht.readTemperature();
      h = dht.readHumidity();
    } else {
      for (int i = 0; i < 9; i++) {
        Serial.print("Probando en ");
        Serial.print(nombresPines[i]);
        Serial.println("...");
        
        DHT dhtPrueba(pinesPosibles[i], DHTTYPE);
        dhtPrueba.begin();
        delay(1500); // Tiempo real que necesita el DHT11 para responder
        
        float tempPrueba = dhtPrueba.readTemperature();
        float humPrueba = dhtPrueba.readHumidity();
        
        if (!isnan(tempPrueba) && !isnan(humPrueba)) {
          pinEncontrado = pinesPosibles[i];
          t = tempPrueba;
          h = humPrueba;
          Serial.println("=========================================");
          Serial.print("¡LISTO! SENSOR DETECTADO EN: ");
          Serial.println(nombresPines[i]);
          Serial.println("=========================================");
          break;
        }
      }
    }

    if (!isnan(t) && !isnan(h)) {
      temperaturaActual = t;
      humedadActual = h;
      
      if (temperaturaActual > UMBRAL_TEMP) {
        alarmaActiva = true;
        digitalWrite(PIN_LED, HIGH);
      } else {
        alarmaActiva = false;
        digitalWrite(PIN_LED, LOW);
        noTone(PIN_BUZZER);
        digitalWrite(PIN_BUZZER, LOW);
      }
      
      Serial.printf("[SENSOR] Temp: %.1f C | Hum: %.1f %% | Alerta: %s | WiFi: %s\n", 
                    temperaturaActual, humedadActual, 
                    alarmaActiva ? "SI" : "NO",
                    (WiFi.status() == WL_CONNECTED) ? "CONECTADO" : "BUSCANDO...");
    } else {
      pinEncontrado = -1;
      Serial.println("[AVISO] No respondio en ningun pin. ¿Esta prendido el foquito rojo del DHT11?");
    }
  }

  if (alarmaActiva) {
    if (tiempoActual - ultimoTiempoAlarma >= INTERVALO_ALARMA) {
      ultimoTiempoAlarma = tiempoActual;
      estadoBuzzer = !estadoBuzzer;
      if (estadoBuzzer) {
        tone(PIN_BUZZER, 2500);
      } else {
        noTone(PIN_BUZZER);
      }
    }
  }

  if (tiempoActual - ultimoTiempoThingSpeak >= INTERVALO_THINGSPEAK) {
    ultimoTiempoThingSpeak = tiempoActual;

    if (WiFi.status() == WL_CONNECTED && temperaturaActual != 0.0) {
      ThingSpeak.setField(1, temperaturaActual);
      ThingSpeak.setField(2, humedadActual);
      int codigoHTTP = ThingSpeak.writeFields(SECRET_CH_ID, SECRET_WRITE_APIKEY);
      if (codigoHTTP == 200) {
        Serial.println("[THINGSPEAK] Datos enviados correctamente (HTTP 200).");
      } else {
        Serial.println("[THINGSPEAK] Error HTTP: " + String(codigoHTTP));
      }
    }
  }
}
