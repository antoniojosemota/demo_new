#include <MFRC522.h> //biblioteca responsável pela comunicação com o módulo RFID-RC522
#include <SPI.h> //biblioteca para comunicação do barramento SPI
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

#define SS_PIN    21
#define RST_PIN   22

#define SIZE_BUFFER     18
#define MAX_SIZE_BLOCK  16

#define PIN_R 33
#define PIN_Y 32
#define PIN_G 25  // trocado de 34 para 25 (pino que aceita saída)
#define PUSHBUTTON 26


const char* ssid = "Familia Brandao ";
const char* password = "994706949";
const char* mqtt_server = "192.168.18.68";
char current_time[129];
bool started = false;
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
unsigned long result = 0;
String serverName = "http://192.168.18.68:3000/api/atividade";
const unsigned long intervalo = 60000; // 1 minuto = 60.000 ms
static unsigned long lastPublish = 0;
String userId = "";  // variável global para armazenar o id do usuário do GET


volatile bool check = false;

HTTPClient http;
WiFiClient espClient;
PubSubClient client(espClient);

void leituraDados();
void my_timer();
void startService();


void startService(String id) {
  if (id == "") {
    Serial.println("ID vazio, não é possível iniciar o serviço");
    return;
  }

  String postEndpoint = "http://192.168.18.68:3000/api/order-services/209493fc-4d4b-4dec-a460-a9460bc8db21/start";
  
  http.begin(postEndpoint);
  http.addHeader("Content-Type", "application/json");
  
  // Corpo do POST, passando o id do usuário
  String jsonPayload = "{\"technicianId\":\"" + id + "\"}";
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    Serial.print("POST iniciado, código: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Resposta: " + response);
  } else {
    Serial.print("Erro no POST: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}

void finishService(String id){
  if (id == "") {
    Serial.println("ID vazio, não é possível iniciar o serviço");
    return;
  }

  String postEndpoint = "http://192.168.18.68:3000/api/order-services/209493fc-4d4b-4dec-a460-a9460bc8db21/complete";
  
  http.begin(postEndpoint);
  http.addHeader("Content-Type", "application/json");
  
  // Corpo do POST, passando o id do usuário
  String jsonPayload = "{\"technicianId\":\"" + id + "\"}";
  
  int httpResponseCode = http.POST(jsonPayload);
  
  if (httpResponseCode > 0) {
    Serial.print("POST iniciado, código: ");
    Serial.println(httpResponseCode);
    String response = http.getString();
    Serial.println("Resposta: " + response);
  } else {
    Serial.print("Erro no POST: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida em [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Exemplo: acende LED se receber "on"
  if (String((char*)payload).startsWith("on")) {
    digitalWrite(2, HIGH);
  } else {
    digitalWrite(2, LOW);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (client.connect("espClient")) {
      Serial.println("conectado");
      client.subscribe("meu/topico"); // Assina o tópico
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5s");
      delay(5000);
    }
  }
}

//esse objeto 'chave' é utilizado para autenticação
MFRC522::MIFARE_Key key;
//código de status de retorno da autenticação
MFRC522::StatusCode status;

// Definicoes pino modulo RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  // Inicia a serial
  Serial.begin(115200);
  SPI.begin(); // Init SPI bus

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_Y, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PUSHBUTTON, INPUT_PULLUP);
  
  // Inicia MFRC522
  mfrc522.PCD_Init(); 
  // Mensagens iniciais no serial monitor
  Serial.println("Aproxime o seu cartao do leitor...");
  Serial.println();

}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    // Lê cartão RFID se presente
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      if (!check) {
        leituraDados();
      }
    }

    // Roda timer do botão
    if (check) {
      my_timer();
    }

    // Publica a cada 1 min
    static unsigned long lastPublish = 0;
    if (millis() - lastPublish >= intervalo) {
      lastPublish = millis();

      unsigned long minutos = (elapsedTime / 1000) / 60;
      char msg[64];
      snprintf(msg, sizeof(msg), "Minutos passados: %lu", minutos);

      client.publish("timer/topico", msg);
      Serial.println(msg);
    }

    // Para o cartão
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
}



//faz a leitura dos dados do cartão/tag
void leituraDados()
{
  mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); 
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  byte buffer[SIZE_BUFFER] = {0};
  char name[17];
  byte bloco = 1;
  byte tamanho = SIZE_BUFFER;

  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, bloco, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) { Serial.println("Falha na autenticação"); return; }

  status = mfrc522.MIFARE_Read(bloco, buffer, &tamanho);
  if (status != MFRC522::STATUS_OK) { Serial.println("Falha na leitura"); return; }

  for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++) {
      Serial.write(buffer[i]);
      name[i] = (char)buffer[i];
  }
  name[16] = '\0';

  char rfid[128];
  snprintf(rfid, sizeof(rfid), "http://192.168.18.68:8080/api/technicians/rfid/%s", name);
  http.begin(rfid);
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    String payload = http.getString();
    Serial.println(payload);

    // Extrair ID do JSON (simples, sem biblioteca externa)
    int idIndex = payload.indexOf("\"id\":\"");
    if (idIndex != -1) {
      int start = idIndex + 6;
      int end = payload.indexOf("\"", start);
      userId = payload.substring(start, end);
      Serial.println("ID capturado: " + userId);
    } else {
      userId = "";
      Serial.println("ID não encontrado no payload");
    }

  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  check = true;
}

void my_timer() {
  if (digitalRead(PUSHBUTTON) == LOW && !started) {
    startTime = millis();
    started = true;

    digitalWrite(PIN_G, HIGH);
    digitalWrite(PIN_Y, LOW);
    digitalWrite(PIN_R, LOW);

    Serial.println("▶️ Cronômetro iniciado!");

    // Faz POST para iniciar serviço
    startService(userId);

    delay(500); // debounce
  }

  if (started) {
    elapsedTime = millis() - startTime;
    Serial.print("Tempo decorrido: ");
    Serial.print(elapsedTime / 1000);
    Serial.println("s");

    if (digitalRead(PUSHBUTTON) == LOW) {
      Serial.println("⏹️ Cronômetro parado.");
      started = false;
      result = (elapsedTime/1000)/60;
      startTime = 0;

      digitalWrite(PIN_G, LOW);
      digitalWrite(PIN_Y, LOW);
      digitalWrite(PIN_R, HIGH);

      Serial.print("⏱️ Tempo final: ");
      Serial.print(result);
      Serial.println("minuto");

      // Se quiser, aqui poderia chamar outro POST para finalizar
      // ex: finishService(userId);

      delay(500); // debounce
    }
  }
}
