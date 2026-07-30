/*
  ESP32-S3 Web Server + TFT ILI9341 Image Display
  نمایش عکس ارسال‌شده از طریق وب‌سرور روی LCD 2.8 اینچ
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ==================== تنظیمات WiFi ====================
const char* ssid = "Amir";
const char* password = "Amir1389117";

// ==================== تعریف اشیاء ====================
TFT_eSPI tft = TFT_eSPI();
WebServer server(80);

// مسیر ذخیره عکس
const char* imagePath = "/upload.jpg";

// ==================== TJpg Decoder Callback ====================
// این تابع برای رندر هر بلوک JPEG روی TFT فراخوانی می‌شه
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

// ==================== صفحه HTML آپلود ====================
const char* uploadPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32-S3 Image Upload</title>
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; background: #1a1a2e; color: #eee; }
    h1 { color: #e94560; }
    .container { max-width: 400px; margin: auto; padding: 20px; border-radius: 15px; background: #16213e; }
    input[type="file"] { margin: 20px 0; }
    button { background: #e94560; color: white; border: none; padding: 12px 30px; 
             border-radius: 25px; cursor: pointer; font-size: 16px; }
    button:hover { background: #ff6b6b; }
    .status { margin-top: 20px; padding: 10px; border-radius: 10px; }
    .success { background: #2ecc71; }
    .error { background: #e74c3c; }
  </style>
</head>
<body>
  <div class="container">
    <h1>📷 Upload Image</h1>
    <p>Resolution: 240x320 | Format: JPEG</p>
    <form id="uploadForm" enctype="multipart/form-data">
      <input type="file" name="image" accept="image/jpeg,image/jpg" required><br>
      <button type="submit">Upload & Display</button>
    </form>
    <div id="status"></div>
  </div>
  <script>
    document.getElementById('uploadForm').onsubmit = async function(e) {
      e.preventDefault();
      const formData = new FormData(this);
      const status = document.getElementById('status');
      status.className = 'status';
      status.innerText = 'Uploading...';
      
      try {
        const res = await fetch('/upload', { method: 'POST', body: formData });
        const text = await res.text();
        status.innerText = text;
        status.className = 'status ' + (res.ok ? 'success' : 'error');
      } catch(err) {
        status.innerText = 'Error: ' + err;
        status.className = 'status error';
      }
    };
  </script>
</body>
</html>
)rawliteral";

// ==================== هندلرها ====================

void handleRoot() {
  server.send(200, "text/html", uploadPage);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.endsWith(".jpg") && !filename.endsWith(".jpeg")) {
      server.send(400, "text/plain", "Error: Only JPEG files allowed!");
      return;
    }
    
    // حذف فایل قبلی
    if (SPIFFS.exists(imagePath)) {
      SPIFFS.remove(imagePath);
    }
    
    // باز کردن فایل جدید برای نوشتن
    File file = SPIFFS.open(imagePath, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Error: Could not create file!");
      return;
    }
    file.close();
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // نوشتن داده‌ها
    File file = SPIFFS.open(imagePath, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    // آپلود تموم شد → نمایش روی TFT
    displayImage();
    server.send(200, "text/plain", "✅ Image uploaded and displayed successfully!");
  }
}

// ==================== نمایش عکس روی TFT ====================
void displayImage() {
  if (!SPIFFS.exists(imagePath)) {
    Serial.println("Image file not found!");
    return;
  }
  
  File file = SPIFFS.open(imagePath, FILE_READ);
  if (!file) {
    Serial.println("Failed to open image file!");
    return;
  }
  
  uint32_t fileSize = file.size();
  Serial.printf("Image size: %d bytes\n", fileSize);
  
  // پاک کردن صفحه
  tft.fillScreen(TFT_BLACK);
  
  // نمایش عکس JPEG
  // x, y = مختصات شروع (مرکز‌چین)
  uint16_t x = 0, y = 0;
  
  // اگه عکس کوچیک‌تر از صفحه باشه، مرکز‌چین کن
  // (TJpg_Decoder خودش سایز رو می‌فهمه)
  
  TJpgDec.drawFsJpg(x, y, imagePath);
  
  file.close();
  Serial.println("Image displayed!");
}

// ==================== setup ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32-S3 TFT Web Server ===");
  
  // --- راه‌اندازی SPIFFS ---
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed! Formatting...");
    SPIFFS.format();
    SPIFFS.begin(true);
  }
  Serial.println("SPIFFS mounted successfully");
  
  // --- راه‌اندازی TFT ---
  tft.init();
  tft.setRotation(2); // 0=Portrait, 1=Landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  
  // نمایش پیام اولیه
  tft.setCursor(10, 10);
  tft.println("ESP32-S3");
  tft.setCursor(10, 40);
  tft.println("Connecting...");
  
  // --- راه‌اندازی TJpg_Decoder ---
  TJpgDec.setJpgScale(1);      // بدون اسکیل (1:1)
  TJpgDec.setSwapBytes(true);  // ESP32 Little Endian
  TJpgDec.setCallback(tft_output);
  
  // --- اتصال WiFi ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi Connection Failed!");
    tft.fillScreen(TFT_RED);
    tft.setCursor(10, 10);
    tft.println("WiFi Failed!");
    return;
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // نمایش IP روی TFT
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("Connected!");
  tft.setCursor(10, 40);
  tft.println("IP:");
  tft.setCursor(10, 70);
  tft.println(WiFi.localIP().toString());
  tft.setCursor(10, 120);
  tft.println("Open browser");
  tft.setCursor(10, 150);
  tft.println("& upload image");
  
  // --- راه‌اندازی وب‌سرور ---
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() {}, handleUpload);
  
  server.begin();
  Serial.println("HTTP Server started");
}

// ==================== loop ====================
void loop() {
  server.handleClient();
  delay(1);
}