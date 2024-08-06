#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

#define API_KEY "AIzaSyB0wk84Ujy458903OmhZ1fCCl2Kxfa_xzM"
#define DATABASE_URL "https://bcs-anti-kebakaran-pencurian-default-rtdb.asia-southeast1.firebasedatabase.app/"

const char *ssid = "AKEBAPEN";
const char *password = "12345678";

// const char *ssid = "2nd Floor_Tanjung";
// const char *password = "salman07";

// const char *ssid = "Ida Bagus Putu Putra Manauaba ";
// const char *password = "12341234";

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

String statusCondition = "aman"; // Default status
int count = 0;
bool signupOK = false;
bool relay_statusAuto = false;
bool relay_statusManual = false;
int lastGasStatus = 0;
bool manualControl = false; // Tambahkan variabel untuk kontrol manual

void connectToWiFi();
void connectToFirebase();
void setupComponents();
void sendDataToFirebase(int gasValue);
void sendAutoRelayStatusToFirebase();
void sendManualRelayStatusToFirebase();
void checkAutoRelayStatusFromFirebase();
void checkManualRelayStatusFromFirebase();
void updateGasSafetyIndicatorManual(int gasValue);
void updateGasSafetyIndicatorAuto(int gasValue);
void updateGasSafetyIndicator(int gasValue);
void sendStatusConditionToFirebase(String statusCondition);



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

  IPAddress staticIP(192, 168, 43, 228); // Tentukan alamat IP statis yang diinginkan
  IPAddress gateway(192, 168, 43, 1);    // Gateway yang sesuai dengan jaringan Anda
  IPAddress subnet(255, 255, 255, 0);    // Subnet mask untuk jaringan Anda

  // Atur alamat IP statis
  WiFi.config(staticIP, gateway, subnet);
}

void loop() {
  checkAutoRelayStatusFromFirebase();
  checkManualRelayStatusFromFirebase(); // Membaca nilai manual_relay_status dari Firebase

  // Baca nilai sensor MQ2
  int gasValue = analogRead(mq2Pin);
  // Cetak nilai sensor ke Serial Monitor
  Serial.print("Nilai Sensor MQ2: ");
  Serial.println(gasValue);
  sendDataToFirebase(gasValue);
  delay(1000);
  
  // Memaksa mengaktifkan relay jika relay_statusAuto true atau mode kontrol manual aktif
  updateGasSafetyIndicator(gasValue);
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
    String autoBoolPath = "/relay_status/device_2/auto_relay_status";
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

void checkManualRelayStatusFromFirebase() {
  if (Firebase.ready() && signupOK) {
    String manualBoolPath = "/relay_status/device_2/manual_relay_status";
    if (Firebase.RTDB.getString(&fbdo, manualBoolPath.c_str())) {
      String relayStatusManualStr = fbdo.stringData(); // Ambil nilai string dari Firebase
      // Konversi nilai string menjadi boolean
      bool relayStatusManualFromFirebase = (relayStatusManualStr == "1");
      
      Serial.print("Membaca nilai relay manual dari Firebase: ");
      Serial.println(relayStatusManualFromFirebase);

      // Perbarui status relay manual lokal jika berhasil membaca dari Firebase
      relay_statusManual = relayStatusManualFromFirebase;
    } else {
      Serial.println("Gagal membaca status relay manual dari Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}



void sendDataToFirebase(int intValue)
{
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0))
  {
    String intPath = "/lpg_concentration/device_2/lpg_concentration_sensor_value";
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
    String autoBoolPath = "/relay_status/device_2/auto_relay_status";
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
    String manualBoolPath = "/relay_status/device_2/manual_relay_status";
    
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

void sendStatusConditionToFirebase(String statusCondition) {
  if (Firebase.ready() && signupOK) {
    String statusPath = "/lpg_concentration/device_2/status";
    if (Firebase.RTDB.setString(&fbdo, statusPath.c_str(), statusCondition)) {
      Serial.println("Status kondisi terkirim ke Firebase: " + statusCondition);
    } else {
      Serial.println("Gagal mengirim status kondisi ke Firebase");
      Serial.println("Alasan: " + fbdo.errorReason());
    }
  }
}



void updateGasSafetyIndicator(int gasValue) {
  if (manualControl) {
    updateGasSafetyIndicatorManual(gasValue);
  } else {
    updateGasSafetyIndicatorAuto(gasValue);
  }
}

void updateGasSafetyIndicatorAuto(int gasValue) {
  int newGasStatus = 0;
  if (gasValue >= BAHAYA_THRESHOLD) {
    relay_statusAuto = true;
    statusCondition = "bahaya";
    digitalWrite(relayPin, HIGH);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
    Serial.println("BAHAYA! Aktifkan relay secara otomatis.");
    newGasStatus = 2;      
  } else if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD) {
    relay_statusAuto = false;
    relay_statusAuto = true;
    statusCondition = "siaga";
    // sendAutoRelayStatusToFirebase(); // Anda dapat menghapus pemanggilan ini jika tidak diperlukan
    updateGasSafetyIndicatorManual(gasValue);
    return;
  } else if (gasValue <= AMAN_THRESHOLD && gasValue < PERINGATAN_THRESHOLD) {
    relay_statusAuto = false;
    relay_statusManual = false;
    statusCondition = "aman";
    digitalWrite(relayPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    Serial.println("AMAN! Matikan relay secara otomatis.");
    newGasStatus = 0;    
    sendManualRelayStatusToFirebase();
  }

  // Memeriksa apakah relay status manual adalah 1 dan nilai gas berada dalam ambang batas aman
  if (relay_statusManual && gasValue <= AMAN_THRESHOLD) {
    digitalWrite(relayPin, HIGH); // Mengaktifkan relay secara paksa menjadi HIGH
    Serial.println("Kondisi aman tetapi relay diaktifkan secara paksa (mode manual).");
  }

  if (lastGasStatus != newGasStatus) {
    lastGasStatus = newGasStatus;
    sendAutoRelayStatusToFirebase(); // Jika Anda membutuhkan pemanggilan ini, Anda dapat mengaktifkannya kembali
    sendDataToFirebase(gasValue);
    sendStatusConditionToFirebase(statusCondition);
  }
}


void updateGasSafetyIndicatorManual(int gasValue) {
  int newGasStatus = 0;
  if (gasValue <= AMAN_THRESHOLD) {
    statusCondition = "aman";
    // Saat kondisi aman, kembali ke mode otomatis
    Serial.println("Kembali ke mode kontrol otomatis karena kondisi aman.");
    manualControl = false; // Matikan kontrol manual
    updateGasSafetyIndicatorAuto(gasValue); // Kembali ke mode otomatis
    return;
  } else if (gasValue >= PERINGATAN_THRESHOLD && gasValue < BAHAYA_THRESHOLD) {
    // Saat kondisi peringatan, relay diatur sesuai dengan status relay manual dari Firebase
    if (relay_statusManual) {
      statusCondition = "bahaya";
      digitalWrite(relayPin, HIGH);
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      Serial.println("PERINGATAN! Aktifkan relay secara manual.");
      newGasStatus = 1;
    } else {
      relay_statusAuto = false;
      statusCondition = "siaga";
      digitalWrite(relayPin, LOW);
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      Serial.println("PERINGATAN! Matikan relay secara manual atau otomatis.");
      newGasStatus = 1;
    }
  }

  if (lastGasStatus != newGasStatus) {
    lastGasStatus = newGasStatus;
    sendAutoRelayStatusToFirebase();
    sendDataToFirebase(gasValue);
    sendStatusConditionToFirebase(statusCondition);
  }
}





