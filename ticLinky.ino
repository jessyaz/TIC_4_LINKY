#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//WiFi Connection configuration
char wifi_name[] = "sensorbucket";     //  le nom du reseau WIFI
char wifi_password[] = "12345678";  // le mot de passe WIFI
//mqtt server
char mqtt_server[] = "192.168.1.227";  //adresse IP serveur mqtt
int mqtt_port = 1883;
char MQTT_USER[] = "zefzef";
char MQTT_PASS[] = "fezfzef";

WiFiClient espClient;
PubSubClient MQTTclient(espClient);

void MQTTconnect() {
  while (!MQTTclient.connected()) {
    String clientId = "TestClient-";
    clientId += String(random(0xffff), HEX);
    if (MQTTclient.connect(clientId.c_str(), "", "")) {
      //Connexion ok
    } else {  // si echec on re-tente la connexion toutes les 5 secondes.
      delay(5000);
    }
  }
}

char HHPHC;
int ISOUSC;               // intensité souscrite
int IINST;                // intensité instantanée    en A
int PAPP;                 // puissance apparente      en VA
unsigned long HCHC;       // compteur Heures Creuses  en W
unsigned long HCHP;       // compteur Heures Pleines  en W
unsigned long BASE;       // index BASE               en W
String PTEC;              // période tarif en cours
String ADCO;              // adresse du compteur
String OPTARIF;           // option tarifaire
int IMAX;                 // intensité maxi = 90A
String MOTDETAT;          // status word
boolean teleInfoReceived;

char chksum(char *buff, uint8_t len);
boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber);

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];

long lastMsg = 0;   //Horodatage du dernier message publié sur MQTT
long lastRecu = 0;
bool debug = false;  //Affiche sur la console si True

int maxtrywificon = 20;
int nbtrywificon = 0;

int maxtrymqttcon = 5;
int nbtrymqttcon = 0;

// ---------------------------------------------- //
//        Basic constructor for LoKyTIC           //
void TeleInfo() {
  // variables initializations
  //  ADCO = "000000000000";
  //  OPTARIF = "----";
  //  ISOUSC = 0;
  //  HCHC = 0L;
  //  HCHP = 0L;
  //  BASE = 0L;
  //  PTEC = "----";
  //  HHPHC = '-';
  //  IINST = 0;
  //  IMAX = 0;
  PAPP = 0;
  //  MOTDETAT = "------";
}

void updateParameters() {
  
  Serial.begin(1200);  // Important!!! -> RESTART LoKyTIC
  teleInfoReceived = readTeleInfo();
  Serial.end(); // Important!!! -> STOP LoKyTIC to send packet.
}

boolean readTeleInfo()  {
#define startFrame  0x02
#define endFrame    0x03
#define startLine   0x0A
#define endLine     0x0D
#define maxFrameLen 280
  int comptChar = 0; // variable de comptage des caractères reçus
  char charIn = 0;  // variable de mémorisation du caractère courant en réception
  char bufferTeleinfo[21] = "";
  int bufferLen = 0;
  int checkSum;
  int sequenceNumnber = 0;    // number of information group
  while (charIn != startFrame) {
    if (Serial.available()) {
      charIn = Serial.read() & 0x7F;
    }
  }
  while (charIn != endFrame)
  {
    if (Serial.available()) {
      charIn = Serial.read() & 0x7F;
      comptChar++;
      if (charIn == startLine)  bufferLen = 0;
      bufferTeleinfo[bufferLen] = charIn;
      if (charIn == endLine)  {
        checkSum = bufferTeleinfo[bufferLen - 1];
        if (chksum(bufferTeleinfo, bufferLen) == checkSum)  {
          strncpy(&bufferTeleinfo[0], &bufferTeleinfo[1], bufferLen - 3);
          bufferTeleinfo[bufferLen - 3] = 0x00;
          sequenceNumnber++;
          if (! handleBuffer(bufferTeleinfo, sequenceNumnber))  {
            return false;
          }
        }
        else  {
          return false;
        }
      }
      else
        bufferLen++;
    }
    if (comptChar > maxFrameLen)  {
      return false;
    }
  }
  return true;
}

boolean handleBuffer(char *bufferTeleinfo, int sequenceNumnber) {
  char* resultString = strchr(bufferTeleinfo, ' ') + 1;
  boolean sequenceIsOK;
  switch (sequenceNumnber) {
    case 1:
      if (sequenceIsOK = bufferTeleinfo[0] == 'A')  ADCO = String(resultString);
      break;
    case 2:
      if (sequenceIsOK = bufferTeleinfo[0] == 'O')  OPTARIF = String(resultString);
      break;
    case 3:
      if (sequenceIsOK = bufferTeleinfo[1] == 'S')  ISOUSC = atol(resultString);
      break;
    case 4:
      if (sequenceIsOK = bufferTeleinfo[3] == 'C') {
#define Linky_HCHP true

#ifdef Linky_HCHP
        HCHC = atol(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[0] == 'B') {
#define Linky_BASE true
#ifdef Linky_BASE
        BASE = atol(resultString);
#endif

      }
      break;

    case 5:
      if (sequenceIsOK = bufferTeleinfo[3] == 'P') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        HCHP = atol(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'T') {
#define Linky_BASE true
#ifdef Linky_BASE
        PTEC = String(resultString);
#endif
      }
      break;

    case 6:
      if (sequenceIsOK = bufferTeleinfo[1] == 'T') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        PTEC = String(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'I') {
#define Linky_BASE true
#ifdef Linky_BASE
        IINST = atol(resultString);
#endif
      }
      break;

    case 7:
      if (sequenceIsOK = bufferTeleinfo[1] == 'I') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        IINST = atol(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'M') {
#define Linky_BASE true
#ifdef Linky_BASE
        IMAX = atol(resultString);
#endif
      }
      break;

    case 8:
      if (sequenceIsOK = bufferTeleinfo[1] == 'M') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        IMAX = atol(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'A') {
#define Linky_BASE true
#ifdef Linky_BASE
        PAPP = atol(resultString);
#endif
      }
      break;

    case 9:
      if (sequenceIsOK = bufferTeleinfo[1] == 'A') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        PAPP = atol(resultString);
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'H') {
#define Linky_BASE true
#ifdef Linky_BASE
        HHPHC = resultString[0];
#endif
      }
      break;

    case 10:
      if (sequenceIsOK = bufferTeleinfo[1] == 'H') {
#define Linky_HCHP true
#ifdef Linky_HCHP
        HHPHC = resultString[0];
#endif
      }
      else if (sequenceIsOK = bufferTeleinfo[1] == 'O') {
#define Linky_BASE true
#ifdef Linky_BASE
        MOTDETAT = String(resultString);
#endif
      }
      break;

    case 11:
      if (sequenceIsOK = bufferTeleinfo[1] == 'O')  MOTDETAT = String(resultString);
      break;

  }
  return sequenceIsOK;
}

char chksum(char *buff, uint8_t len)  {
  int i;
  char sum = 0;
  for (i = 1; i < (len - 2); i++)
    sum = sum + buff[i];
  sum = (sum & 0x3F) + 0x20;
  return (sum);
}

void setup() {
  Serial.begin(1200);
  WiFi.begin(wifi_name, wifi_password); // Lancement de la connexion WIFI
  while (WiFi.status() != WL_CONNECTED) { // Test de la connexion WIFI

    delay(500); // On bloque le programme tant que la wifi n'est pas établie.
  }
  MQTTclient.setServer(mqtt_server, mqtt_port); //On set le serveur MQTT, cf lib PubSubClient.h
}

void loop() {
  static uint32_t lastTimeMqtt = 0;
  if (!MQTTclient.connected()) { //Connexion ou reconnexion au serveur MQTT
    MQTTconnect();
  }
  TeleInfo();
  MQTTclient.loop();

  String topic_alive = "winky/alive";
  String cc = "Im alive";
  String cc2 = "Im 2";
  String cc3 = "Im 3";
  MQTTclient.publish(topic_alive.c_str(),  cc.c_str());

  delay(1000);
  while (PAPP == 0) {
    updateParameters();
  }
  MQTTclient.publish(topic_alive.c_str(),  cc2.c_str());

  String topic_power = "winky/power";
  String topic_base = "winky/base";
  String topic_hc = "winky/hc";
  String topic_hp = "winky/hp";
  
  
  MQTTclient.publish(topic_power.c_str(), String(PAPP).c_str());
  MQTTclient.publish(topic_base.c_str(), String(BASE).c_str());
  MQTTclient.publish(topic_hc.c_str(), String(HCHC).c_str());
  MQTTclient.publish(topic_hp.c_str(), String(HCHP).c_str());

  delay(1000);
  MQTTclient.publish(topic_alive.c_str(),  cc3.c_str());

  ESP.reset();
}
