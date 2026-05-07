// Program 03 - Program ini khusus merekam Kurva Reaksi Sistem (Open-Loop Step Response)
// Data tercetak dalam format CSV (waktu_ms,pwm,rpm) setiap 10ms.
// Data ini diolah untuk menentukan nilai kP kI kD yg optimal terhadap respon motor
#include <Wire.h>
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;

// Konfigurasi Pin Perangkat Keras
const int PIN_ENA = 10;
const int PIN_IN1 = 9;
const int PIN_IN2 = 8;

// Parameter Motor Tereduksi (Ganti Ke setelah kalibrasi)
const float SET_PWM = 200;          // ubah nilai ke PWM yg ditentukan. (0-255)
const float R_MOTOR = 12.53;        // ohm (Hasil pengukuran rerata DCR)
const float Ke_MOTOR = 0.00129;     // Vemf / RPM (Hasil hitung Ke)

// --- FUNGSI Kalibrasi Data - sesuaikan dari program 01 --- //
// Masukkan hasil rumus kalibrasi sensor dengan pembanding multimeter.
// Menggunakan regresi linear tanpa intersep (C=0) untuk menjaga titik nol pembacaan tetap nol.
float kalibrasiV(float rawV) {
  return rawV * 1.0281;
}

float kalibrasiI(float rawI) {
  return rawI * 0.8441;
}

// --- Variabel Hasil Pembacaan ---
float tegangan_V = 0;
float arus_mA = 0;

// Variabel Sistem Pengujian dan pewaktu
bool modeUji = false;
unsigned long waktuMulaiUji = 0;
unsigned long waktuSampelTerakhir = 0;
int pwmUji = 0;
const unsigned long INTERVAL_SAMPEL = 50;   // Sampel 50ms
const unsigned long BATAS_WAKTU = 5000;     // Henti otomatis 5 detik

// Fungsi menjalankan motor berdasarkan nilai PWM
void jalankanMotor(int nilaiPWM) {
  // Batasi sinyal kontrol ke pwm 8-bit
  nilaiPWM = constrain(nilaiPWM, 0, 255);
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  analogWrite(PIN_ENA, nilaiPWM);
}

// Mengubah register internal INA219 untuk mengaktifkan 128x sampel hardware averaging.
// Waktu konversi internal ~68ms Menghasilkan data sensor yg stabil murni.
// Config 0x07FF datasheet register bisa dibaca di halaman 19-20
// https://www.alldatasheet.net/html-pdf/2192938/TI2/INA219/1353/20/INA219.html
// Set resolusi sensor INA219 ke tingkat tertinggi, pilihan : 32V_2A, 32V_1A, 16V_400mA
void setHardwareAveragingMaksimal() {
  ina219.setCalibration_16V_400mA();
  Wire.beginTransmission(0x40);         // Alamat I2C INA219
  Wire.write(0x00);                     // Register 0x00 adalah register config
  Wire.write(0x07);                     // Config LSB
  Wire.write(0xFF);                     // Config MSB
  Wire.endTransmission();               // Tutup komunikasi
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(1); }  // Menunggu Serial Monitor siap.

  if (!ina219.begin()) {
    Serial.println("Error: INA219 tidak terdeteksi.\nPeriksa koneksi kabel I2C.");
    delay(1000);
    asm volatile ("jmp 0");  //--- Kode untuk reset system, jump pointer ke baris 0
  }
  setHardwareAveragingMaksimal();

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  
  Serial.println("Sistem Siap.");
  Serial.println("Kirim perintah 'uji=200' untuk mulai pengujian step respons.");
  Serial.println("Kirim perintah 'stop' untuk henti darurat.");
  Serial.println("--- MULAI DATA CSV ---");
  Serial.println("waktu_ms,pwm,rpm");
}

void loop() {
  // Pembacaan Perintah Serial
  if (Serial.available() > 0) {
    String perintah = Serial.readStringUntil('\n');
    perintah.trim();
    perintah.toLowerCase();

    if (perintah.startsWith("uji=")) {
      pwmUji = perintah.substring(4).toInt();
      modeUji = true;
      waktuMulaiUji = millis();
      waktuSampelTerakhir = millis();
      jalankanMotor(pwmUji);
    } else if (perintah == "stop") {
      modeUji = false;
      jalankanMotor(0);
      Serial.println("Uji dihentikan manual.");
    }
  }

  // Mode Pengambilan Data Eksekusi Diskrit
  if (modeUji == true) {
    unsigned long waktuSekarang = millis();
    unsigned long waktuBerjalan = waktuSekarang - waktuMulaiUji;

    // Keamanan: Hentikan motor setelah 5 detik
    if (waktuBerjalan > BATAS_WAKTU) {
      modeUji = false;
      jalankanMotor(0);
      Serial.println("--- SELESAI ---");
      return;
    }

    // Pencetakan Data 50ms
    if (waktuSekarang - waktuSampelTerakhir >= INTERVAL_SAMPEL) {
      waktuSampelTerakhir = waktuSekarang;

      // Akuisisi data mentah dari sensor dan terapkan faktor kalibrasi
      tegangan_V = kalibrasiV(ina219.getBusVoltage_V());
      arus_mA = kalibrasiI(ina219.getCurrent_mA());
      float arus_Amper = arus_mA / 1000.0;

      // Kalkulasi RPM dengan estimasi Sensorless
      float teganganEMF = tegangan_V - (arus_Amper * R_MOTOR);
      float estimasiRPM = teganganEMF / Ke_MOTOR;

      if (estimasiRPM < 0) estimasiRPM = 0;

      // Cetak format CSV standar
      Serial.print(waktuBerjalan);
      Serial.print(",");
      Serial.print(pwmUji);
      Serial.print(",");
      Serial.println(estimasiRPM);
    }
  }
}