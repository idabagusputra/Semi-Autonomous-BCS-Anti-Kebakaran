#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define API_KEY "AIzaSyB0wk84Ujy458903OmhZ1fCCl2Kxfa_xzM"
#define DATABASE_URL "https://bcs-anti-kebakaran-pencurian-default-rtdb.asia-southeast1.firebasedatabase.app/"

const char *ssid = "2nd Floor_Tanjung";
const char *password = "salman07";

const int relayPin = D8;
const int mq2Pin = A0;
const int redLedPin = D4;
const int yellowLedPin = D3;
const int greenLedPin = D2;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define AMAN_THRESHOLD 499    // Nilai ambang batas untuk keadaan aman
#define PERINGATAN_THRESHOLD 500 // Nilai ambang batas untuk keadaan peringatan
#define BAHAYA_THRESHOLD 700    // Nilai ambang batas untuk keadaan peringatan

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;
bool relayStatusAuto = false;
bool relayStatusManual = true;
int lastGasStatus = 0;

bool manualControl = false; // Tambahkan variabel untuk kontrol manual

void connectToWiFi();
void connectToFirebase();
void setupComponents();
void updateGasSafetyIndicator(int gasValue);
void sendDataToFirebase(int gasValue);
void sendRelayStatusToFirebase();
void manualControlRelay();
void checkFirebaseRelayStatus();

void setup()
{
  Serial.begin(115200);
  connectToWiFi();
  connectToFirebase();
  setupComponents();
}

void setupComponents()
{
  pinMode(relayPin, OUTPUT);
  pinMode(mq2Pin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
}

void loop()
{
  checkFirebaseRelayStatus();  // Pindahkan ke bagian awal loop
  // Baca nilai sensor MQ2
  int gasValue = analogRead(mq2Pin);
  // Cetak nilai sensor ke Serial Monitor
  Serial.print("Nilai Sensor MQ2: ");
  Serial.println(gasValue);
  delay(1000);

  // Baca status relay dari Firebase hanya jika tidak dalam mode kontrol manual
  if (!manualControl && Firebase.ready() && signupOK)
  {
    String autoBoolPath = "/RelayStatus/autoRelayStatus";
    if (Firebase.RTDB.getBool(&fbdo, autoBoolPath.c_str()))
    {
      relayStatusAuto = fbdo.boolData();
    }
    else
    {
      Serial.println("Failed to get auto relay status from Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }
  }

  // Memaksa mengaktifkan relay jika relayStatusAuto true atau mode kontrol manual aktif
  if (relayStatusManual == true || manualControl)
  {
    manualControlRelay(); // Panggil fungsi kontrol manual
  }
  else
  {
    updateGasSafetyIndicator(gasValue);
  }
}

void connectToWiFi()
{
  Serial.println();
  Serial.print("Menghubungkan ke WiFi");

  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, HIGH);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Terhubung");
  Serial.print("Alamat IP: ");
  Serial.println(WiFi.localIP());

  digitalWrite(greenLedPin, HIGH);
  digitalWrite(redLedPin, LOW);
}

#define USER_EMAIL "ibp.putra.m@gmail.com"
#define USER_PASSWORD "Dayu1808!"
String uid;

void connectToFirebase()
{
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);

  config.token_status_callback = tokenStatusCallback;

  config.max_token_generation_retry = 5;

  Firebase.begin(&config, &auth);

  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('.');
    delay(1000);
  }

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);

  signupOK = true;
}

void updateGasSafetyIndicator(int gasValue)
{
  int newGasStatus = 0;
  if (gasValue >= BAHAYA_THRESHOLD)
  {
    relayStatusAuto = true;
    digitalWrite(relayPin, HIGH);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
    Serial.println("BAHAYA! Aktifkan relay.");
    newGasStatus = 2;
  }
  else if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD)
  {
    if (!manualControl && relayStatusAuto)
    {
      // Peringatan: Gas level is in warning range, turn off relay
      relayStatusAuto = false;
      digitalWrite(relayPin, LOW);
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      Serial.println("PERINGATAN! Matikan relay.");
      newGasStatus = 1;
    }
    else if (manualControl && relayStatusManual)
    {
      relayStatusManual = true;
      digitalWrite(relayPin, HIGH);
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      Serial.println("PERINGATAN! Matikan relay.");
      newGasStatus = 1;
    }
  }
  else if (gasValue <= AMAN_THRESHOLD && gasValue < PERINGATAN_THRESHOLD)
  {
    relayStatusAuto = false;
    digitalWrite(relayPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    Serial.println("AMAN! Matikan relay.");
    newGasStatus = 0;
  }

  if (lastGasStatus != newGasStatus)
  {
    lastGasStatus = newGasStatus;
    sendRelayStatusToFirebase();
    sendDataToFirebase(gasValue);
  }
}



void sendDataToFirebase(int intValue)
{
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    String intPath = "/LPG_Concentration/int";
    if (Firebase.RTDB.setInt(&fbdo, intPath.c_str(), intValue))
    {
      Serial.println("Integer data sent to Firebase successfully");
      Serial.println("Path: " + intPath);
    }
    else
    {
      Serial.println("Failed to send integer data to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }
  }
}

void sendRelayStatusToFirebase()
{
  delay(3000);
  if (Firebase.ready() && signupOK)
  {
    String autoBoolPath = "/RelayStatus/autoRelayStatus";
    if (Firebase.RTDB.setBool(&fbdo, autoBoolPath.c_str(), relayStatusAuto))
    {
      Serial.println("Auto relay status sent to Firebase successfully");
      Serial.println("Path: " + autoBoolPath);
    }
    else
    {
      Serial.println("Failed to send auto relay status to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }

    String manualBoolPath = "/RelayStatus/manualRelayStatus";
    if (Firebase.RTDB.setBool(&fbdo, manualBoolPath.c_str(), relayStatusManual))
    {
      Serial.println("Manual relay status sent to Firebase successfully");
      Serial.println("Path: " + manualBoolPath);
    }
    else
    {
      Serial.println("Failed to send manual relay status to Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }
  }
}

void checkFirebaseRelayStatus()
{
  if (Firebase.ready() && signupOK)
  {
    String autoBoolPath = "/RelayStatus/autoRelayStatus";
    if (Firebase.RTDB.getBool(&fbdo, autoBoolPath.c_str()))
    {
      relayStatusAuto = fbdo.boolData();
      Serial.print("Membaca nilai relay otomatis dari Firebase: ");
      Serial.println(relayStatusAuto);

      if (relayStatusAuto)
      {
        Serial.println("Auto RelayStatus diubah menjadi true");
      }
      else
      {
        Serial.println("Auto RelayStatus diubah menjadi false");
      }
    }
    else
    {
      Serial.println("Gagal membaca status relay otomatis dari Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }

    String manualBoolPath = "/RelayStatus/manualRelayStatus";
    if (Firebase.RTDB.getBool(&fbdo, manualBoolPath.c_str()))
    {
      relayStatusManual = fbdo.boolData();
      Serial.print("Membaca nilai relay manual dari Firebase: ");
      Serial.println(relayStatusManual);

      if (relayStatusManual)
      {
        Serial.println("Manual RelayStatus diubah menjadi true");
      }
      else
      {
        Serial.println("Manual RelayStatus diubah menjadi false");
      }
    }
    else
    {
      Serial.println("Gagal membaca status relay manual dari Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}

void manualControlRelay()
{
  int gasValue = analogRead(mq2Pin);

  // Aktifkan relay secara paksa saat dalam mode manual, bahkan dalam keadaan peringatan (lampu kuning)
  if (relayStatusManual)
  {
    digitalWrite(relayPin, HIGH);
    Serial.println("Mode Kontrol Manual Aktif - Aktifkan Relay Secara Paksa");
    updateGasSafetyIndicator(gasValue);

    // Matikan relay secara otomatis saat dalam keadaan aman (lampu hijau)
    if (gasValue <= AMAN_THRESHOLD)
    {
      digitalWrite(relayPin, LOW);
      Serial.println("Mode Kontrol Manual Aktif - Matikan Relay Secara Otomatis");
      relayStatusManual = false;
      sendRelayStatusToFirebase();
    }
    else
    {
      relayStatusManual = false;
      sendRelayStatusToFirebase();
    }
  }
}



