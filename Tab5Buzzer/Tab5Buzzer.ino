#include <arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>

M5GFX display;

int push = 0;


void setup()
{
    uint32_t colors = display.color888(0, 0, 0);
    
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("Start Buzzer...");
    
    M5.Lcd.setTextDatum(4);  // 0（左上）～8（右下）
    M5.Lcd.setRotation(1);   // 90度回転（0〜3）
    
    display = M5.Display;
    
    // 表示セットアップ
    display.init();
    display.setFont(&fonts::lgfxJapanGothic_40);
    display.startWrite();
    display.fillScreen(DARKGREEN);
    display.setTextColor(GREEN, DARKGREEN);
    display.setTextScroll(false);
    display.setTextSize(2); 
    display.setRotation(1);
    
    display.waitDisplay();
    
    display.setCursor(1280/2-display.textWidth("タッチでブザーオンオフ")/2, 200);  // 画面中央
    display.printf("タッチでブザーオンオフ");
    
    display.setCursor(1280/2-display.textWidth("ブザー停止")/2, 720/2);  // 画面中央
    display.printf("ブザー停止");
    
    display.display();
    
    M5.Speaker.setVolume(255);  // 音量最大
    
}


void loop()
{
    // ボタン状態取得
    lgfx::touch_point_t tp[3];
    int nums = display.getTouchRaw(tp, 3);
    
    if(nums != 0){
        // 画面タッチ中
        if(push == 0){
            // ブザー開始
            display.waitDisplay();
            display.setCursor(1280/2-display.textWidth("ブザー開始")/2, 720/2);
            display.printf("ブザー開始");
            display.display();
            Serial.println("ブザー開始");
        }
        push = 1;
        
        // 発音
        M5.Speaker.tone(1000, 500);
        delay(500+300);
    }
    else{
        if(push != 0){
            // ブザー停止
            display.waitDisplay();
            display.setCursor(1280/2-display.textWidth("ブザー停止")/2, 720/2);
            display.printf("ブザー停止");
            display.display();
            Serial.println("ブザー停止");
            delay(100);
        }
        push = 0;
    }
    
}

