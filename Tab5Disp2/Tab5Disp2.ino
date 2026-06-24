#include <arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <time.h>

int syear, smonth, sday, shour, smin, ssec;  // 表示中の時刻
int mandet = 0;  // 人検知
int buzzer = 0;  // ブザー
int sens[5] = { 0, 0, 0, 0, 0};  // センサー

M5GFX display;
int rtc = 0;

int mode = 0;     // 0=時計動作 1=年 2=月 3=日 4=時 5=分 6=秒
int nowpos = -1;  // 押されたボタン番号（0～3）放された時に処理
int nowx = -1;    // 画面上のタッチ位置
int nowy = -1;    // 画面上のタッチ位置


// RTCに頻繁にアクセスするとハングするので60分間隔で時間補正を行う
unsigned long tms = 0;  // システム経過時間（msec）1秒計測用
unsigned long tmm = 0;  // システム経過時間（msec）60分計測用

// 時計ボタン座標 サイズ200x100 ボタン間隔10
int timebtn[4][4] = {
    {225, 180, 200, 100},
    {435, 180, 200, 100},
    {645, 180, 200, 100},
    {855, 180, 200, 100}
};

// 人検知ボタン
int manbtn[2][4] = {
    {225, 320, 200, 100},
    {435, 320, 200, 100}
};

// センサーボタン
int sensbtn[5][4] = {
    {475, 460, 100, 100},
    {595, 460, 100, 100},
    {715, 460, 100, 100},
    {835, 460, 100, 100},
    {955, 460, 100, 100}
};


void setup()
{
    uint32_t colors = display.color888(64, 64, 64);
    
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("Start Setup...");
    
    if(!M5.Rtc.isEnabled()){
        Serial.println("ERROR : RTC not Found!");
    }
    else{
        //M5.Rtc.setDateTime( { { 2026, 1, 1 }, { 12, 0, 0 } } );
        syear  = 2026;
        smonth = 1;
        sday   = 1;
        shour  = 12;
        smin   = 0;
        ssec   = 0;
        M5.Rtc.setDateTime({{syear, smonth, sday}, {shour, smin, ssec}});
        
        Serial.println("Set to RTC = 2026/1/1 12:00:00");
        rtc = 1;
    }
    
    M5.Lcd.setTextDatum(4);  // 0（左上）～8（右下）
    M5.Lcd.setRotation(1);   // 90度回転（0〜3）
    
    display = M5.Display;
    
    // 表示セットアップ
    display.init();
    display.setFont(&fonts::lgfxJapanGothic_40);
    display.startWrite();
    display.fillScreen(display.color888(0, 64, 0));
    display.setTextColor(GREEN);
    display.setTextScroll(false);
    display.setTextSize(2); 
    display.setRotation(1);
    
    display.waitDisplay();
    
    colors = display.color888(64, 64, 64);
    display.fillRect(0, 0, display.width(), display.height(), colors);
    
    display.setCursor(225, 80+40);
    if(rtc == 1){
        auto dt = M5.Rtc.getDateTime();
        display.printf("%04d/%02d/%02d %02d:%02d:%02d", 
            dt.date.year, dt.date.month, dt.date.date,
            dt.time.hours, dt.time.minutes, dt.time.seconds
        );
    }
    else{
        display.printf("ERROR : RTC NOT FOUND !");
    }
    
    display.setTextSize(1); 
    display.setCursor(225, 460+50);
    display.printf(" センサー：");
    
    // 色サンプル表示
    int x = 60;
    display.fillRect(x, 600, 60, 60, LIGHTGREY);   x+=65;  // 明灰
    display.fillRect(x, 600, 60, 60, DARKGREY);    x+=65;  // 暗灰
    display.fillRect(x, 600, 60, 60, BLUE);        x+=65;  // 明青
    display.fillRect(x, 600, 60, 60, NAVY);        x+=65;  // 暗青
    display.fillRect(x, 600, 60, 60, GREEN);       x+=65;  // 明緑
    display.fillRect(x, 600, 60, 60, DARKGREEN);   x+=65;  // 暗緑
    display.fillRect(x, 600, 60, 60, YELLOW);      x+=65;  // 明黄
    display.fillRect(x, 600, 60, 60, OLIVE);       x+=65;  // 暗黄
    display.fillRect(x, 600, 60, 60, ORANGE);      x+=65;  // 橙
    display.fillRect(x, 600, 60, 60, GREENYELLOW); x+=65;  // 黄緑
    display.fillRect(x, 600, 60, 60, MAGENTA);     x+=65;  // 明紫
    display.fillRect(x, 600, 60, 60, PURPLE);      x+=65;  // 暗紫
    display.fillRect(x, 600, 60, 60, PINK);        x+=65;  // ピンク
    display.fillRect(x, 600, 60, 60, CYAN);        x+=65;  // 明水
    display.fillRect(x, 600, 60, 60, DARKCYAN);    x+=65;  // 暗水
    display.fillRect(x, 600, 60, 60, RED);         x+=65;  // 明赤
    display.fillRect(x, 600, 60, 60, MAROON);      x+=65;  // 暗赤
    display.fillRect(x, 600, 60, 60, WHITE);       x+=65;  // 白
    
    display.display();
    
    drawbtn(0, BLACK, GREEN,    "設 定");
    drawbtn(1, BLACK, DARKGREY, "切 替");
    drawbtn(2, BLACK, DARKGREY, "＜");
    drawbtn(3, BLACK, DARKGREY, "＞");
    
    drawbtn(10, BLACK, LIGHTGREY, "人検知オフ");
    drawbtn(11, BLACK, LIGHTGREY, "ブザーオフ");
    
    drawbtn(20, BLACK, LIGHTGREY, "1");
    drawbtn(21, BLACK, LIGHTGREY, "2");
    drawbtn(22, BLACK, LIGHTGREY, "3");
    drawbtn(23, BLACK, LIGHTGREY, "4");
    drawbtn(24, BLACK, LIGHTGREY, "5");
    
    tms = millis();
    tmm = millis();
    
}


void loop()
{
    unsigned long tmsnow = 0;
    
    // ボタン状態取得
    lgfx::touch_point_t tp[3];
    int nums = display.getTouchRaw(tp, 3);
    
    // タッチあり
    checkbutton(nums, tp[0].x, tp[0].y, &mode);
    
    if(mode == 0){  // 時間表示中
        tmsnow = millis();
        if((tmsnow - tms) >= 1000 || tmsnow < tmm){
            // 先回から1秒経過 -> 時計更新
            tms += 1000;
            addone(&syear, &smonth, &sday, &shour, &smin, &ssec);
            
            if((tmsnow - tmm) >= 3600000 || tmsnow < tmm){
                // 先回から60分経過 or millis()オーバーフロー -> RTCで時刻補正
                if(tmsnow > tmm){
                    tmm += 3600000;
                }
                else{
                    tmm = tmsnow;  // オーバーフロー時
                }
                if(rtc == 1){  // RTCあり
                    auto dt = M5.Rtc.getDateTime();
                    syear  = dt.date.year;
                    smonth = dt.date.month;
                    sday   = dt.date.date;
                    shour  = dt.time.hours;
                    smin   = dt.time.minutes;
                    ssec   = dt.time.seconds;
                }
                // 時刻描画
                timedisp(syear, smonth, sday, shour, smin, ssec, mode);
                
                Serial.println("!");
            }
            else{
                // 時刻描画
                timedisp(syear, smonth, sday, shour, smin, ssec, mode);
                
                if(ssec == 0){
                    Serial.print(".");
                }
            }
        }
    }
    
    //delay(100); // 0.1秒待機
}


// 時刻加算（日時に1秒加算）
void addone(int *yr, int *mn, int *dt, int *hh, int *mm, int *ss)
{
    struct tm t;
    struct tm *tm_info;
    
    t.tm_year = *yr - 1900;    // 年（1900 からの経過年）
    t.tm_mon  = *mn - 1;       // 月（0 = 1月）
    t.tm_mday = *dt;           // 日
    t.tm_hour = *hh;           // 時
    t.tm_min  = *mm;           // 分
    t.tm_sec  = *ss;           // 秒
    
    time_t unixTime = mktime(&t);  // UNIX時間（秒）
    
    unixTime += 1;
    
    tm_info = localtime(&unixTime);
    
    *yr = tm_info->tm_year + 1900;
    *mn = tm_info->tm_mon + 1;
    *dt = tm_info->tm_mday;
    *hh = tm_info->tm_hour;
    *mm = tm_info->tm_min;
    *ss = tm_info->tm_sec;
}


// 時刻描画
void timedisp(int yr, int mn, int dt, int hh, int mm, int ss, int md)
{
    display.waitDisplay();
    
    uint32_t colors = display.color888(64, 64, 64);
    display.fillRect(225, 80-5, 900, 90, colors);
    display.setTextSize(2); 
    display.setTextColor(GREEN);
    display.setCursor(225, 80+40);
    display.printf("%04d/%02d/%02d %02d:%02d:%02d", 
        yr, mn, dt, hh, mm, ss
    );
    
    // edit mode
    underbar(md);
    
    display.display();
}


// 編集モード時のアンダーバー表示
// md : 0=時計動作 1=年 2=月 3=日 4=時 5=分 6=秒
void underbar(int md)
{
    char str[20];
    switch(md){
        case 0 : strcpy(str, "                   "); break;
        case 1 : strcpy(str, "____               "); break;
        case 2 : strcpy(str, "     __            "); break;
        case 3 : strcpy(str, "        __         "); break;
        case 4 : strcpy(str, "           __      "); break;
        case 5 : strcpy(str, "              __   "); break;
        case 6 : strcpy(str, "                 __"); break;
        default : break;
    }
    display.setTextColor(YELLOW);
    display.setCursor(225, 80+40);
    display.printf(str);
}


// ボタン描画
// num : 時計ボタン（0～3）人検知（10、11）センサー（20～24）
// col1 : 背景色
// col2 : 文字色
// str : 表示文字
void drawbtn(int num, int col1, int col2, char *str)
{
    display.waitDisplay();
    
    // ボタン
    display.setTextSize(1); 
    display.setTextColor(col1);
    
    if(num < 10){
        display.fillRect(timebtn[num][0], timebtn[num][1], timebtn[num][2], timebtn[num][3], col2);
        int w = display.textWidth(str);
        display.setCursor(timebtn[num][0]+timebtn[num][2]/2-w/2, timebtn[num][1]+timebtn[num][3]/2);
        display.printf(str);
    }
    else if(num < 20){
        display.fillRect(manbtn[num-10][0], manbtn[num-10][1], manbtn[num-10][2], manbtn[num-10][3], col2);
        int w = display.textWidth(str);
        display.setCursor(manbtn[num-10][0]+manbtn[num-10][2]/2-w/2, manbtn[num-10][1]+manbtn[num-10][3]/2);
        display.printf(str);
    }
    else if(num < 30){
        display.fillRect(sensbtn[num-20][0], sensbtn[num-20][1], sensbtn[num-20][2], sensbtn[num-20][3], col2);
        int w = display.textWidth(str);
        display.setCursor(sensbtn[num-20][0]+sensbtn[num-20][2]/2-w/2, sensbtn[num-20][1]+sensbtn[num-20][3]/2);
        display.printf(str);
    }
    
    display.display();
}


// 制御ボタンエリア
// md : 0=時計動作 1=年 2=月 3=日 4=時 5=分 6=秒
void checkbutton(int nums, int x, int y, int *md)
{
    int pos;
    
    int xx = y;  // 画面タッチは左下が原点（描画系は左上が原点）
    int yy = 719 - x;
    
    if(nums == 0){
        // リリース（ボタン確定）
        if(nowpos >= 0){
            // 横座標がボタン位置
            if(nowpos == 0){  // 時計設定
                if(*md == 0){
                    *md = 1;   // 設定モード
                    drawbtn(0, BLACK, YELLOW, "設 定");
                    drawbtn(1, BLACK, CYAN,   "切 替");
                    drawbtn(2, BLACK, CYAN,   "＜");
                    drawbtn(3, BLACK, CYAN,   "＞");
                    
                    if(mandet == 0){
                        drawbtn(10, BLACK, DARKGREY, "人検知オフ");
                    }
                    else{
                        drawbtn(10, BLACK, DARKCYAN, "人検知オン");
                    }
                    if(buzzer == 0){
                        drawbtn(11, BLACK, DARKGREY, "ブザーオフ");
                    }
                    else{
                        drawbtn(11, BLACK, DARKCYAN, "ブザーオン");
                    }
                    
                    if(sens[0] == 0){
                        drawbtn(20, BLACK, DARKGREY, "1");
                    }
                    else{
                        drawbtn(20, BLACK, MAROON, "1");
                    }
                    if(sens[1] == 0){
                        drawbtn(21, BLACK, DARKGREY, "2");
                    }
                    else{
                        drawbtn(21, BLACK, NAVY, "2");
                    }
                    if(sens[2] == 0){
                        drawbtn(22, BLACK, DARKGREY, "3");
                    }
                    else{
                        drawbtn(22, BLACK, DARKGREEN, "3");
                    }
                    if(sens[3] == 0){
                        drawbtn(23, BLACK, DARKGREY, "4");
                    }
                    else{
                        drawbtn(23, BLACK, OLIVE, "4");
                    }
                    if(sens[4] == 0){
                        drawbtn(24, BLACK, DARKGREY, "5");
                    }
                    else{
                        drawbtn(24, BLACK, PURPLE, "5");
                    }
                    
                    // 時刻描画（カーソル移動）
                    timedisp(syear, smonth, sday, shour, smin, ssec, *md);
                }
                else{
                    // 設定実行
                    // 設定月に31日が無い対応（閏年非対応）
                    if(smonth==2 || smonth==4 || smonth==6 || smonth==9 || smonth==11){
                        if(sday > 30){
                            sday = 30;
                        }
                    }
                    
                    if(rtc ==1){
                        M5.Rtc.setDateTime({{syear, smonth, sday}, {shour, smin, ssec}});
                    }
                    tms = millis();
                    tmm = millis();
                    
                    drawbtn(0, BLACK, GREEN,    "設 定");
                    drawbtn(1, BLACK, DARKGREY, "切 替");
                    drawbtn(2, BLACK, DARKGREY, "＜");
                    drawbtn(3, BLACK, DARKGREY, "＞");
                    *md = 0;   // 時計モード
                    
                    if(mandet == 0){
                        drawbtn(10, BLACK, LIGHTGREY, "人検知オフ");
                    }
                    else{
                        drawbtn(10, BLACK, CYAN, "人検知オン");
                    }
                    if(buzzer == 0){
                        drawbtn(11, BLACK, LIGHTGREY, "ブザーオフ");
                    }
                    else{
                        drawbtn(11, BLACK, CYAN, "ブザーオン");
                    }
                    
                    if(sens[0] == 0){
                        drawbtn(20, BLACK, LIGHTGREY, "1");
                    }
                    else{
                        drawbtn(20, BLACK, RED, "1");
                    }
                    if(sens[1] == 0){
                        drawbtn(21, BLACK, LIGHTGREY, "2");
                    }
                    else{
                        drawbtn(21, BLACK, BLUE, "2");
                    }
                    if(sens[2] == 0){
                        drawbtn(22, BLACK, LIGHTGREY, "3");
                    }
                    else{
                        drawbtn(22, BLACK, GREEN, "3");
                    }
                    if(sens[3] == 0){
                        drawbtn(23, BLACK, LIGHTGREY, "4");
                    }
                    else{
                        drawbtn(23, BLACK, ORANGE, "4");
                    }
                    if(sens[4] == 0){
                        drawbtn(24, BLACK, LIGHTGREY, "5");
                    }
                    else{
                        drawbtn(24, BLACK, MAGENTA, "5");
                    }
                    
                }
            }
            else if(nowpos == 1){  // 切替
                if(*md > 0){
                    *md += 1;
                    if(*md > 6){
                        *md = 1;
                    }
                    
                    // 時刻描画（カーソル移動）
                    timedisp(syear, smonth, sday, shour, smin, ssec, *md);
                }
            }
            else if(nowpos == 2){  // 「＜」
                if(*md == 1){  // 年
                    if(syear > 2000){
                        syear -= 1;
                    }
                }
                else if(*md == 2){  // 月
                    if(smonth > 1){
                        smonth -= 1;
                    }
                    else{
                        smonth = 12;
                    }
                }
                else if(*md == 3){  // 日
                    if(sday > 1){
                        sday -= 1;
                    }
                    else{
                        sday = 31;
                    }
                }
                else if(*md == 4){  // 時
                    if(shour > 0){
                        shour -= 1;
                    }
                    else{
                        shour = 23;
                    }
                }
                else if(*md == 5){  // 分
                    if(smin > 0){
                        smin -= 1;
                    }
                    else{
                        smin = 59;
                    }
                }
                else if(*md == 6){  // 秒
                    if(ssec > 0){
                        ssec -= 1;
                    }
                    else{
                        ssec = 59;
                    }
                }
                if(*md > 0){
                    timedisp(syear, smonth, sday, shour, smin, ssec, *md);
                }
            }
            else if(nowpos == 3){  // 「＞」
                if(*md == 1){  // 年
                    if(syear < 2100){
                        syear += 1;
                    }
                }
                else if(*md == 2){  // 月
                    if(smonth < 12){
                        smonth += 1;
                    }
                    else{
                        smonth = 1;
                    }
                }
                else if(*md == 3){  // 日
                    if(sday < 31){
                        sday += 1;
                    }
                    else{
                        sday = 1;
                    }
                }
                else if(*md == 4){  // 時
                    if(shour < 23){
                        shour += 1;
                    }
                    else{
                        shour = 0;
                    }
                }
                else if(*md == 5){  // 分
                    if(smin < 59){
                        smin += 1;
                    }
                    else{
                        smin = 0;
                    }
                }
                else if(*md == 6){  // 秒
                    if(ssec < 59){
                        ssec += 1;
                    }
                    else{
                        ssec = 0;
                    }
                }
                if(*md > 0){
                    timedisp(syear, smonth, sday, shour, smin, ssec, *md);
                }
            }
            else if(nowpos == 10){  // 人検知オンオフ
                if(mandet == 0){
                    mandet = 1;
                    drawbtn(10, BLACK, CYAN, "人検知オン");
                }
                else{
                    mandet = 0;
                    drawbtn(10, BLACK, LIGHTGREY, "人検知オフ");
                }
            }
            else if(nowpos == 11){  // ブザーオンオフ
                if(buzzer == 0){
                    buzzer = 1;
                    drawbtn(11, BLACK, CYAN, "ブザーオン");
                }
                else{
                    buzzer = 0;
                    drawbtn(11, BLACK, LIGHTGREY, "ブザーオフ");
                }
            }
            else if(nowpos == 20){  // センサー1オンオフ
                if(sens[0] != 0){
                    sens[0] = 0;
                    drawbtn(20, BLACK, LIGHTGREY, "1");
                }
                else{
                    sens[0] = 1;
                    drawbtn(20, BLACK, RED, "1");
                }
            }
            else if(nowpos == 21){  // センサー2オンオフ
                if(sens[1] != 0){
                    sens[1] = 0;
                    drawbtn(21, BLACK, LIGHTGREY, "2");
                }
                else{
                    sens[1] = 1;
                    drawbtn(21, BLACK, BLUE, "2");
                }
            }
            else if(nowpos == 22){  // センサー3オンオフ
                if(sens[2] != 0){
                    sens[2] = 0;
                    drawbtn(22, BLACK, LIGHTGREY, "3");
                }
                else{
                    sens[2] = 1;
                    drawbtn(22, BLACK, GREEN, "3");
                }
            }
            else if(nowpos == 23){  // センサー4オンオフ
                if(sens[3] != 0){
                    sens[3] = 0;
                    drawbtn(23, BLACK, LIGHTGREY, "4");
                }
                else{
                    sens[3] = 1;
                    drawbtn(23, BLACK, ORANGE, "4");
                }
            }
            else if(nowpos == 24){  // センサー5オンオフ
                if(sens[4] != 0){
                    sens[4] = 0;
                    drawbtn(24, BLACK, LIGHTGREY, "5");
                }
                else{
                    sens[4] = 1;
                    drawbtn(24, BLACK, MAGENTA, "5");
                }
            }
        }
        nowpos = -1;
    }
    else{
        if(*md == 0){
            // 時計設定開始 or 人検知 or センサー
            // 時計設定か？
            if(yy > timebtn[0][1] && yy < timebtn[0][1]+timebtn[0][3]){
                // 縦座標がボタン位置
                if(xx > timebtn[0][0] && xx < timebtn[3][0]+timebtn[3][2]){
                    // 横座標がボタン位置
                    pos = (xx - timebtn[0][0]) / 215;
                    if(pos >= 0 && pos < 4){
                        nowpos = pos;
                        nowx = xx;
                        nowy = yy;
                    }
                }
            }
            // 人検知設定か？
            if(yy > manbtn[0][1] && yy < manbtn[0][1]+manbtn[0][3]){
                // 縦座標がボタン位置
                if(xx > manbtn[0][0] && xx < manbtn[1][0]+manbtn[1][2]){
                    // 横座標がボタン位置
                    pos = (xx - manbtn[0][0]) / 215;
                    if(pos >= 0 && pos < 2){
                        nowpos = pos + 10;
                        nowx = xx;
                        nowy = yy;
                    }
                }
            }
            // センサー設定か？
            if(yy > sensbtn[0][1] && yy < sensbtn[0][1]+sensbtn[0][3]){
                // 縦座標がボタン位置
                if(xx > sensbtn[0][0] && xx < sensbtn[4][0]+sensbtn[4][2]){
                    // 横座標がボタン位置
                    pos = (xx - sensbtn[0][0]) / 116;
                    if(pos >= 0 && pos < 5){
                        nowpos = pos + 20;
                        nowx = xx;
                        nowy = yy;
                    }
                }
            }
        }
        else{
            // 時計設定/解除 押下中（ボタン判定）
            if(yy > timebtn[0][1] && yy < timebtn[0][1]+timebtn[0][3]){
                // 縦座標がボタン位置
                if(xx > timebtn[0][0] && xx < timebtn[3][0]+timebtn[3][2]){
                    // 横座標がボタン位置
                    pos = (xx - timebtn[0][0]) / 215;
                    if(pos >= 0 && pos < 4){
                        nowpos = pos;
                        nowx = xx;
                        nowy = yy;
                    }
                }
            }
        }
    }
}
