// for M5Stacl Tab5
// シリアル通信受信
#include <M5GFX.h>
#include <M5Unified.h>

M5GFX display;

void setup()
{
    uint32_t colors = display.color888(0, 0, 0);
    
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("Start Recieve...");
    
    Serial2.begin(115200, SERIAL_8N1, 53, 54);
    
    M5.Lcd.setTextDatum(4);  // 0（左上）～8（右下）
    M5.Lcd.setRotation(1); // 90度回転（0〜3）
    
    display = M5.Display;
    
    // 表示セットアップ
    display.init();
    display.setFont(&fonts::lgfxJapanGothic_40);
    display.startWrite();
    display.fillScreen(display.color888(0, 64, 0));
    display.setTextColor(GREEN);
    display.setTextScroll(true);
    display.setTextSize(1); 
    display.setRotation(1);
    
    display.waitDisplay();
    colors = display.color888(0, 0, 0);
    display.fillRect(0, 0, display.width(), display.height(), colors);
    display.display();
    
    display.waitDisplay();
    display.setCursor(0, 10);
    display.printf("Start Recieve...\n");
    display.display();
}

void loop()
{
    if (Serial2.available()) {
        String text = Serial2.readStringUntil(0x0a);
        // 受信文字列長が0以上で「IOT」から始まる文字列の時だけ処理する
        if (text.length() > 0 && text.startsWith("IOT")) {
            text.trim();
            //text.replace("*","");
            
            Serial.println(text);
            
            display.waitDisplay();
            display.println(text); //.c_str());
            display.display();
        }
    }

    delay(100); // 0.1秒待機
}

