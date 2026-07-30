/*
  ESP32-S3 Web Server + TFT ILI9341 Image Display
  نمایش عکس + ذخیره دائمی + صفحه اولیه
*/

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ==================== تنظیمات WiFi ====================
const char* ssid = "your_wifi_name";
const char* password = "your_password";

// ==================== تعریف اشیاء ====================
TFT_eSPI tft = TFT_eSPI();
WebServer server(80);

// مسیر ذخیره عکس
const char* imagePath = "/upload.jpg";
const char* savedImagePath = "/saved.jpg";  // عکس ذخیره‌شده دائمی

// تایمر
uint32_t bootTime = 0;
bool showingBootScreen = true;

// ==================== TJpg Decoder Callback ====================
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
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32-S3 Image Upload</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { 
      font-family: 'Segoe UI', Arial, sans-serif; 
      text-align: center; 
      margin-top: 30px; 
      background: linear-gradient(135deg, #1a1a2e, #16213e); 
      color: #eee; 
      min-height: 100vh;
      padding: 20px;
    }
    h1 { color: #e94560; margin-bottom: 10px; }
    .container { 
      max-width: 420px; 
      margin: auto; 
      padding: 25px; 
      border-radius: 20px; 
      background: #0f3460;
      box-shadow: 0 10px 30px rgba(0,0,0,0.3);
    }
    .preview {
      width: 100%;
      max-width: 240px;
      height: 320px;
      background: #1a1a2e;
      border-radius: 10px;
      margin: 15px auto;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #666;
      font-size: 14px;
      overflow: hidden;
    }
    .preview img {
      max-width: 100%;
      max-height: 100%;
      border-radius: 10px;
    }
    input[type="file"] { 
      margin: 15px 0; 
      color: white;
      padding: 10px;
      background: #1a1a2e;
      border-radius: 10px;
      width: 100%;
    }
    .btn-group {
      display: flex;
      gap: 10px;
      justify-content: center;
      flex-wrap: wrap;
    }
    button { 
      background: #e94560; 
      color: white; 
      border: none; 
      padding: 12px 25px; 
      border-radius: 25px; 
      cursor: pointer; 
      font-size: 15px;
      transition: all 0.3s;
      flex: 1;
      min-width: 120px;
    }
    button:hover { 
      background: #ff6b6b; 
      transform: translateY(-2px);
      box-shadow: 0 5px 15px rgba(233, 69, 96, 0.4);
    }
    button.save-btn {
      background: #2ecc71;
    }
    button.save-btn:hover {
      background: #27ae60;
      box-shadow: 0 5px 15px rgba(46, 204, 113, 0.4);
    }
    button.clear-btn {
      background: #34495e;
    }
    .status { 
      margin-top: 15px; 
      padding: 12px; 
      border-radius: 10px; 
      font-size: 14px;
      display: none;
    }
    .status.show { display: block; }
    .success { background: #2ecc71; color: white; }
    .error { background: #e74c3c; color: white; }
    .info { background: #3498db; color: white; }
    .timer {
      color: #e94560;
      font-size: 12px;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>📷 Smart Frame</h1>
    <p style="color: #aaa; font-size: 13px;">ESP32-S3 + ILI9341 240x320</p>
    
    <div class="preview" id="preview">
      <span>No image</span>
    </div>
    
    <form id="uploadForm" enctype="multipart/form-data">
      <input type="file" name="image" accept="image/jpeg,image/jpg" id="fileInput" required>
      <div class="btn-group">
        <button type="submit">📤 Upload</button>
        <button type="button" class="save-btn" onclick="saveImage()">💾 Save</button>
        <button type="button" class="clear-btn" onclick="clearImage()">🗑️ Clear</button>
      </div>
    </form>
    
    <div id="status" class="status"></div>
    <div class="timer" id="timer"></div>
  </div>
  
  <script>
    let savedImageUrl = '';
    
    function showStatus(msg, type) {
      const status = document.getElementById('status');
      status.innerText = msg;
      status.className = 'status show ' + type;
      setTimeout(() => status.className = 'status', 3000);
    }
    
    // پیش‌نمایش فایل انتخابی
    document.getElementById('fileInput').onchange = function() {
      const file = this.files[0];
      if (file) {
        const url = URL.createObjectURL(file);
        document.getElementById('preview').innerHTML = '<img src="' + url + '">';
      }
    };
    
    // آپلود
    document.getElementById('uploadForm').onsubmit = async function(e) {
      e.preventDefault();
      const formData = new FormData(this);
      showStatus('Uploading...', 'info');
      
      try {
        const res = await fetch('/upload', { method: 'POST', body: formData });
        const text = await res.text();
        showStatus(text, res.ok ? 'success' : 'error');
        
        if (res.ok) {
          // آپدیت پیش‌نمایش با عکس جدید
          savedImageUrl = '/current.jpg?t=' + Date.now();
          document.getElementById('preview').innerHTML = '<img src="' + savedImageUrl + '">';
        }
      } catch(err) {
        showStatus('Error: ' + err, 'error');
      }
    };
    
    // ذخیره عکس
    async function saveImage() {
      showStatus('Saving...', 'info');
      try {
        const res = await fetch('/save', { method: 'POST' });
        const text = await res.text();
        showStatus(text, res.ok ? 'success' : 'error');
      } catch(err) {
        showStatus('Error: ' + err, 'error');
      }
    }
    
    // پاک کردن
    async function clearImage() {
      showStatus('Clearing...', 'info');
      try {
        const res = await fetch('/clear', { method: 'POST' });
        const text = await res.text();
        showStatus(text, res.ok ? 'success' : 'error');
        document.getElementById('preview').innerHTML = '<span>No image</span>';
      } catch(err) {
        showStatus('Error: ' + err, 'error');
      }
    }
    
    // لود عکس فعلی
    async function loadCurrentImage() {
      try {
        const res = await fetch('/current.jpg?t=' + Date.now());
        if (res.ok) {
          const blob = await res.blob();
          const url = URL.createObjectURL(blob);
          document.getElementById('preview').innerHTML = '<img src="' + url + '">';
        }
      } catch(e) {
        console.log('No current image');
      }
    }
    
    // لود اولیه
    loadCurrentImage();
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
    
    File file = SPIFFS.open(imagePath, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Error: Could not create file!");
      return;
    }
    file.close();
    
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    File file = SPIFFS.open(imagePath, FILE_APPEND);
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
    
  } else if (upload.status == UPLOAD_FILE_END) {
    // آپلود تموم شد → نمایش روی TFT
    displayImage();
    server.send(200, "text/plain", "✅ Image uploaded and displayed!");
  }
}

// ✅ NEW: ذخیره عکس دائمی
void handleSave() {
  if (!SPIFFS.exists(imagePath)) {
    server.send(400, "text/plain", "Error: No image to save!");
    return;
  }
  
  // حذف فایل ذخیره‌شده قبلی
  if (SPIFFS.exists(savedImagePath)) {
    SPIFFS.remove(savedImagePath);
  }
  
  // کپی فایل
  File source = SPIFFS.open(imagePath, FILE_READ);
  File dest = SPIFFS.open(savedImagePath, FILE_WRITE);
  
  if (!source || !dest) {
    server.send(500, "text/plain", "Error: File operation failed!");
    return;
  }
  
  // کپی با بافر
  uint8_t buffer[512];
  size_t bytesRead;
  while ((bytesRead = source.read(buffer, sizeof(buffer))) > 0) {
    dest.write(buffer, bytesRead);
  }
  
  source.close();
  dest.close();
  
  Serial.println("Image saved permanently!");
  server.send(200, "text/plain", "✅ Image saved! Will persist after reboot.");
}

// ✅ NEW: پاک کردن عکس
void handleClear() {
  if (SPIFFS.exists(imagePath)) {
    SPIFFS.remove(imagePath);
  }
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 100);
  tft.println("Image Cleared!");
  tft.setCursor(10, 140);
  tft.println("Upload new image");
  
  server.send(200, "text/plain", "✅ Image cleared!");
}

// ✅ NEW: سرو عکس فعلی
void handleCurrentImage() {
  if (!SPIFFS.exists(imagePath)) {
    server.send(404, "text/plain", "No image");
    return;
  }
  
  File file = SPIFFS.open(imagePath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Error");
    return;
  }
  
  server.streamFile(file, "image/jpeg");
  file.close();
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
  file.close();
  
  // پاک کردن صفحه
  tft.fillScreen(TFT_BLACK);
  
  // نمایش عکس JPEG
  TJpgDec.drawFsJpg(0, 0, imagePath);
  
  Serial.println("Image displayed!");
}

// ✅ NEW: نمایش عکس ذخیره‌شده
void displaySavedImage() {
  if (!SPIFFS.exists(savedImagePath)) {
    Serial.println("No saved image found");
    return;
  }
  
  Serial.println("Loading saved image...");
  tft.fillScreen(TFT_BLACK);
  TJpgDec.drawFsJpg(0, 0, savedImagePath);
  Serial.println("Saved image displayed!");
}

// ✅ NEW: صفحه اولیه Connecting
void showBootScreen() {
  tft.fillScreen(TFT_BLACK);
  
  // افکت loading
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(40, 80);
  tft.println("Connecting...");
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(50, 140);
  tft.println("ESP32-S3 Smart Frame");
  tft.setCursor(50, 160);
  tft.println("ILI9341 240x320");
  
  // دایره loading
  tft.drawCircle(120, 200, 20, TFT_CYAN);
}

// ✅ NEW: نمایش IP
void showIPScreen() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(30, 60);
  tft.println("Connected!");
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.println("IP:");
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 140);
  tft.println(WiFi.localIP().toString());
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(30, 200);
  tft.println("Open browser & upload");
  tft.setCursor(50, 220);
  tft.println("image in 10 sec...");
}

// ==================== setup ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n=== ESP32-S3 Smart Frame ===");
  
  // --- راه‌اندازی SPIFFS ---
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed! Formatting...");
    SPIFFS.format();
    SPIFFS.begin(true);
  }
  Serial.println("SPIFFS mounted successfully");
  
  // --- راه‌اندازی TFT ---
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  
  // راه‌اندازی TJpg_Decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  
  // ✅ صفحه اولیه
  showBootScreen();
  
  // --- اتصال WiFi ---
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // انیمیشن نقطه‌ها روی TFT
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    int x = 120 + (attempts % 4) * 15;
    tft.setCursor(x, 200);
    tft.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi Connection Failed!");
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.println("WiFi Failed!");
    tft.setCursor(10, 140);
    tft.println("Check settings");
    return;
  }
  
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // ✅ نمایش IP
  showIPScreen();
  
  // ✅ ذخیره زمان بوت
  bootTime = millis();
  showingBootScreen = true;
  
  // --- راه‌اندازی وب‌سرور ---
  server.on("/", HTTP_GET, handleRoot);
  server.on("/upload", HTTP_POST, []() {}, handleUpload);
  server.on("/save", HTTP_POST, handleSave);           // ✅ NEW
  server.on("/clear", HTTP_POST, handleClear);          // ✅ NEW
  server.on("/current.jpg", HTTP_GET, handleCurrentImage); // ✅ NEW
  
  server.begin();
  Serial.println("HTTP Server started");
}

// ==================== loop ====================
void loop() {
  server.handleClient();
  
  // ✅ بعد از 10 ثانیه، عکس ذخیره‌شده رو نشون بده
  if (showingBootScreen && millis() - bootTime > 10000) {
    showingBootScreen = false;
    
    // اول چک کن آیا عکس آپلودی هست
    if (SPIFFS.exists(imagePath)) {
      Serial.println("Showing uploaded image...");
      displayImage();
    } 
    // اگه نه، عکس ذخیره‌شده رو نشون بده
    else if (SPIFFS.exists(savedImagePath)) {
      Serial.println("Showing saved image...");
      displaySavedImage();
    }
    else {
      // هیچ عکسی نیست
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(20, 100);
      tft.println("No image saved!");
      tft.setCursor(20, 140);
      tft.println("Upload from web");
    }
  }
  
  delay(1);
}
