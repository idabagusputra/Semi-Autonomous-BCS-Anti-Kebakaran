#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define API_KEY "AIzaSyAl_eUmTrRauQGE7xeRPBcehvK8wtAvnzo"
#define DATABASE_URL "https://bcs-akebapen-us-central-default-rtdb.firebaseio.com/"

const char *ssid = "2nd Floor_Tanjung";
const char *password = "salman07";

const int relayPin = D8;
const int mq2Pin = A0;
const int redLedPin = D7;
const int yellowLedPin = D5;
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
bool relay_statusAuto = false;
bool relay_statusManual = true;
int lastGasStatus = 0;

bool manualControl = false; // Tambahkan variabel untuk kontrol manual

void connectToWiFi();
void connectToFirebase();
void setupComponents();
void updateGasSafetyIndicator(int gasValue);
void sendDataToFirebase(int gasValue);
void sendAutoRelayStatusToFirebase();
void sendManualRelayStatusToFirebase();
void checkAutoRelayStatusFromFirebase();
void checkManualRelayStatusFromFirebase();
void manualControlRelay();
void checkManualRelayStatusFromFirebase();

void setup()
{
  Serial.begin(115200);
  pinMode(redLedPin, OUTPUT);
  digitalWrite(redLedPin, HIGH); // Lampu merah menyala saat booting
  connectToWiFi();
  setupComponents();
  connectToFirebase();
}

void setupComponents()
{
  pinMode(relayPin, OUTPUT);
  pinMode(mq2Pin, INPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  digitalWrite(yellowLedPin, HIGH);
}

void loop()
{
  checkAutoRelayStatusFromFirebase();
  checkManualRelayStatusFromFirebase(); // Membaca nilai manual_relay_status dari Firebase

  // Baca nilai sensor MQ2
  int gasValue = analogRead(mq2Pin);
  // Cetak nilai sensor ke Serial Monitor
  Serial.print("Nilai Sensor MQ2: ");
  Serial.println(gasValue);
  sendDataToFirebase(gasValue);
  delay(1000);

  // Baca status relay dari Firebase hanya jika tidak dalam mode kontrol manual
  if (!manualControl && Firebase.ready() && signupOK)
  {
    String autoBoolPath = "/relay_status/auto_relay_status";
    if (Firebase.RTDB.getBool(&fbdo, autoBoolPath.c_str()))
    {
      relay_statusAuto = fbdo.boolData();
    }
    else
    {
      Serial.println("Failed to get auto relay status from Firebase");
      Serial.println("Reason: " + fbdo.errorReason());
    }
  }

  // Memaksa mengaktifkan relay jika relay_statusAuto true atau mode kontrol manual aktif
  if (relay_statusManual || manualControl)
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

  digitalWrite(redLedPin, LOW); // Lampu merah padam setelah terhubung WiFi
  digitalWrite(yellowLedPin, HIGH);
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
  }

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);
  digitalWrite(greenLedPin, HIGH); // Lampu hijau menyala setelah terhubung Firebase
  digitalWrite(yellowLedPin, LOW); // Lampu kuning padam setelah terhubung Firebase
  signupOK = true;
}

void checkAutoRelayStatusFromFirebase()
{
  if (Firebase.ready() && signupOK)
  {
    String autoBoolPath = "/relay_status/auto_relay_status";
    if (Firebase.RTDB.getBool(&fbdo, autoBoolPath.c_str()))
    {
      relay_statusAuto = fbdo.boolData();
      Serial.print("Membaca nilai relay otomatis dari Firebase: ");
      Serial.println(relay_statusAuto);

      if (relay_statusAuto)
      {
        Serial.println("Auto relay_status diubah menjadi true");
      }
      else
      {
        Serial.println("Auto relay_status diubah menjadi false");
      }
    }
    else
    {
      Serial.println("Gagal membaca status relay otomatis dari Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}

void checkManualRelayStatusFromFirebase()
{
  if (Firebase.ready() && signupOK)
  {
    String manualBoolPath = "/relay_status/manual_relay_status";
    if (Firebase.RTDB.getString(&fbdo, manualBoolPath.c_str())) // Menggunakan getString() untuk mendapatkan nilai sebagai string
    {
      String manualString = fbdo.stringData(); // Ambil string dari Firebase

      // Konversi string menjadi boolean
      bool relayStatus = (manualString == "1") ? true : false;

      Serial.print("Membaca nilai relay manual dari Firebase: ");
      Serial.println(relayStatus);

      // Simpan nilai relayStatus jika diperlukan untuk penggunaan selanjutnya
      relay_statusManual = relayStatus;

      // Jika nilai manual_relay_status false, matikan relay
      if (!relayStatus)
      {
        digitalWrite(relayPin, LOW);
        Serial.println("Matikan Relay karena nilai manual_relay_status false.");
      }
    }
    else
    {
      Serial.println("Gagal membaca status relay manual dari Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}

void manualControlRelay() {
  int gasValue = analogRead(mq2Pin);

  // Jika relay sedang dalam mode kontrol manual, relay akan selalu aktif
  if (relay_statusManual) {
    digitalWrite(relayPin, HIGH); // Aktifkan relay
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    Serial.println("Mode Kontrol Manual Aktif - Aktifkan Relay");
    if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD) {
      updateGasSafetyIndicator(gasValue); // Perbarui indikator keamanan gas saat dalam kondisi bahaya
    }

  } else {
    // Jika relay tidak dalam mode kontrol manual
    digitalWrite(relayPin, LOW); // Matikan relay
    Serial.println("Mode Kontrol Manual Non-Aktif - Matikan Relay");
    updateGasSafetyIndicator(gasValue); // Perbarui indikator keamanan gas
  }
}

void sendDataToFirebase(int intValue)
{
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    String intPath = "/lpg_concentration/lpg_concentration_1";
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

void sendAutoRelayStatusToFirebase() {
  if (Firebase.ready() && signupOK) {
    String autoBoolPath = "/relay_status/auto_relay_status";
    if (Firebase.RTDB.setBool(&fbdo, autoBoolPath.c_str(), relay_statusAuto)) {
      Serial.println("Status relay otomatis terkirim ke Firebase: " + String(relay_statusAuto));
    } else {
      Serial.println("Gagal mengirim status relay otomatis ke Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}

void sendManualRelayStatusToFirebase() {
  if (Firebase.ready() && signupOK) {
    String manualBoolPath = "/relay_status/manual_relay_status";
    
    // Konversi nilai boolean menjadi string "0" atau "1"
    String manualBoolValue = relay_statusManual ? "1" : "0";
    
    if (Firebase.RTDB.setString(&fbdo, manualBoolPath.c_str(), manualBoolValue)) {
      Serial.println("Status relay manual terkirim ke Firebase: " + manualBoolValue);
    } else {
      Serial.println("Gagal mengirim status relay manual ke Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}






void updateGasSafetyIndicatorAuto(int gasValue) {
  int newGasStatus = 0;
  if (gasValue >= BAHAYA_THRESHOLD) {
    relay_statusAuto = true;
    relay_statusManual = true; // Ubah hanya jika tidak dalam kontrol manual
    digitalWrite(relayPin, HIGH);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
    Serial.println("BAHAYA! Aktifkan relay.");
    newGasStatus = 2;
  } else if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD) {
    relay_statusAuto = false;
    relay_statusManual = false;
    digitalWrite(relayPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    Serial.println("PERINGATAN! Matikan relay.");
    newGasStatus = 1;
  } else if (gasValue <= AMAN_THRESHOLD && gasValue < PERINGATAN_THRESHOLD) {
    relay_statusAuto = false;
    relay_statusManual = false;
    digitalWrite(relayPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    Serial.println("AMAN! Matikan relay.");
    newGasStatus = 0;
  }

  if (lastGasStatus != newGasStatus) {
    lastGasStatus = newGasStatus;
    sendAutoRelayStatusToFirebase();
    sendManualRelayStatusToFirebase();
  }
}


void updateGasSafetyIndicatorManual(int gasValue) {
  int newGasStatus = 0;
  if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD) {
    if (manualControl && relay_statusManual) {
      relay_statusManual = true;
      digitalWrite(relayPin, HIGH);
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      Serial.println("PERINGATAN! Matikan relay.");
      newGasStatus = 1;
    }
  }

  if (lastGasStatus != newGasStatus) {
    lastGasStatus = newGasStatus;
    sendDataToFirebase(gasValue);
  }
}

void updateGasSafetyIndicator(int gasValue) {
  if (manualControl) {
    updateGasSafetyIndicatorManual(gasValue);
  } else {
    updateGasSafetyIndicatorAuto(gasValue);
  }
}









