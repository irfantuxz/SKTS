// Program 02 - Basic pembacaan sensor INA219 & Write PWM motor
// digunakan setelah mengkalibrasi pembacaan tegangan dan arus sensor INA219
// kemudian ujicoba nilai R_MOTOR dan Ke_MOTOR
// masukkan hasil hitung ke const R_MOTOR dan Ke_MOTOR kemudian bandingkan eRPM dengan tacometer
#include <Wire.h>
#include <Adafruit_INA219.h>
Adafruit_INA219 ina219;

// Konfigurasi Pin Perangkat Keras
const int PIN_ENA = 10;
const int PIN_IN1 = 9;
const int PIN_IN2 = 8;

// Parameter Motor Tereduksi (Ganti Ke setelah kalibrasi perhitungan)
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

// --- Variabel Pewaktuan ---
unsigned long waktuCetakSebelumnya = 0;
const unsigned long INTERVAL_CETAK = 500;
// --- Variabel Hasil Pembacaan ---
float tegangan_V = 0;
float arus_mA = 0;

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
  jalankanMotor(SET_PWM);
}

void loop() {
  unsigned long waktuSekarang = millis();

  // Mode Non-Blocking: Eksekusi setiap INTERVAL_CETAK 500ms
  if (waktuSekarang - waktuCetakSebelumnya >= INTERVAL_CETAK) {
    waktuCetakSebelumnya = waktuSekarang;

    // Akuisisi data mentah dari sensor dan terapkan faktor kalibrasi
    tegangan_V = kalibrasiV(ina219.getBusVoltage_V());
    arus_mA = kalibrasiI(ina219.getCurrent_mA());
    float arus_Amper = arus_mA / 1000.0;
    
    // Kalkulasi RPM dengan estimasi Sensorless
    float teganganEMF = tegangan_V - (arus_Amper * R_MOTOR);
    float estimasiRPM = teganganEMF / Ke_MOTOR;

    // Cetak data ke Serial Monitor untuk dibandingkan dengan multimeter dan tacometer
    Serial.print("V= "); 
    Serial.print(tegangan_V, 2); 
    Serial.print("V");
    Serial.print(",\t");

    Serial.print("I= "); 
    Serial.print(arus_mA, 2); 
    Serial.print("mA");
    Serial.print(",\t");

    Serial.print("vEmf= ");
    Serial.print(teganganEMF);
    Serial.print("V");
    Serial.print(",\t");

    Serial.print("eRPM= ");
    Serial.print(estimasiRPM, 0);
    Serial.print(" RPM");
    Serial.println();
  }
}