#include <arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <time.h>

M5GFX display;
int rtc = 0;

int mode = 0;     // 0=時計動作 1=年 2=月 3=日 4=時 5=分 6=秒
int nowpos = -1;  // 押されたボタン番号（0～3）放された時に処理
int nowx = -1;    // 画面上のタッチ位置
int nowy = -1;    // 画面上のタッチ位置

int syear, smonth, sday, shour, smin, ssec;  // 表示中の時刻
int dscale[32];  // 日スケール
int hscale[32];  // 時スケール
int tcount = 0;

int mandet = 0;  // 人検知
int buzzer = 0;  // ブザー
int sens[5] = { 0, 0, 0, 0, 0};  // センサー

// RTCに頻繁にアクセスするとハングするので60分間隔で時間補正を行う
unsigned long tms = 0;  // システム経過時間（msec）1秒計測用
unsigned long tmm = 0;  // システム経過時間（msec）60分計測用

// 設定ボタン 1280-64-160-20-160, 10, 160, 100
int setupbtn[4] = {876, 10, 160, 100};
// 人検知表示 1280-64-200, 10, 160, 100
int mandisp[4] = {1056, 10, 160, 100};
// 時刻表示 64, 10+40/2
int timepos[2] = {64, 30};
// グラフ表示（グラフ線幅分+2）
int graph[4] = {64, 160, 1152+2, 480};
// 温湿度表示
int temppos[2] = {64, 690};

// 温湿度データ（10倍）4日分×24時間×4回(15分刻み）＝384
int data[5][2][384];
int datanew[5] = {-1, -1, -1, -1, -1};

// 最新の取得データ
int nowdata[2] = {0, 0};

void setup()
{
    int i;
    
    for(i=0; i<384; i++){
        data[0][0][i] = 0;
        data[0][1][i] = 0;
    }
    
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("Start Disp...");
    
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
    //display.fillScreen(display.color888(0, 64, 0));
    display.fillScreen(DARKGREY);
    display.setTextColor(GREEN, DARKGREY);
    display.setTextScroll(false);
    display.setTextSize(2); 
    display.setRotation(1);
    
    display.waitDisplay();
    
    // グラフエリア描画
    graphdraw();
    
    // 時刻表示
    display.setTextSize(1); 
    display.setTextColor(GREEN, DARKGREY);
    display.setCursor(timepos[0], timepos[1]);
    if(rtc == 1){
        auto dt = M5.Rtc.getDateTime();
        display.printf("%04d/%02d/%02d %02d:%02d:%02d", 
            dt.date.year, dt.date.month, dt.date.date,
            //wd[dt.date.weekDay],
            dt.time.hours, dt.time.minutes, dt.time.seconds
        );
    }
    else{
        display.printf("ERROR : RTC NOT FOUND !");
    }
    
    // 温湿度表示
    display.setTextSize(1); 
    display.setCursor(temppos[0], temppos[1]);
    display.printf("現在 16.8℃ 55％ : 最高 20.5℃ 60％ / 最低 10.6℃ 35％");
    
    // 設定ボタン
    display.setTextSize(1); 
    display.setTextColor(BLACK);
    display.fillRect(setupbtn[0], setupbtn[1], setupbtn[2], setupbtn[3], DARKGREEN);
    display.setCursor(setupbtn[0]+setupbtn[2]/2-display.textWidth("設 定")/2, setupbtn[1]+setupbtn[3]/2);
    display.printf("設 定");
    
    // 人検知表示
    display.setTextSize(1); 
    display.setTextColor(DARKGREY);
    display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], DARKCYAN);
    display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth("人検知")/2, mandisp[1]+mandisp[3]/2);
    display.printf("人検知");
    
    display.display();
    
    tms = millis();
    tmm = millis();
    
}


// グラフ再描画
// グラフエリア全ての更新（3時間分左シフト）
void graphdraw()
{
    int x, y;
    int y2;
    int i;
    
    //colors = display.color888(0, 0, 0);
    // 線幅拡大用に縦下方向も拡大
    display.fillRect(graph[0], graph[1], graph[2], graph[3]+2, BLACK);
    // 縦軸（温湿度）
    for(y=0; y<=60; y+=10){
        display.drawLine(graph[0], graph[1]+y*8, graph[0]+graph[2], graph[1]+y*8, LIGHTGREY);
    }
    for(y=5; y<60; y+=10){
        display.drawLine(graph[0], graph[1]+y*8, graph[0]+graph[2], graph[1]+y*8, DARKGREY);
    }
    for(y=0; y<60; y+=10){
        display.setTextSize(1); 
        display.setTextColor(LIGHTGREY, DARKGREY);
        display.setCursor(graph[0]+graph[2]+5, graph[1]+y*8);
        display.printf("%d", 50-y);  // 温度
    }
    display.setCursor(graph[0]+graph[2]+5, graph[1]+graph[3]);
    display.printf("℃");
    for(y=0; y<=100; y+=20){
        display.setTextSize(1); 
        display.setTextColor(LIGHTGREY, DARKGREY);
        if(y==0) x = 0;
        else if(y==100) x = 40;
        else x = 20;
        display.setCursor(x, graph[1]+y*4);
        display.printf("%d", 100-y);  // 湿度
    }
    display.setCursor(22, graph[1]+graph[3]);
    display.printf("％");
    
    // スケール（6時間刻み）
    timescale(syear, smonth, sday, shour, smin, ssec);
    display.fillRect(graph[0], graph[1]-28, graph[2], 28, DARKGREY);
    for(i=1; i<32; i++){
        if(hscale[i] == 0){
            // 日表示
            dispscale(i, 0);
        }
        else if((hscale[i] % 6) == 0){
            // 6時間刻み
            dispscale(i, 1);
        }
    }
    // 縦線
    for(i=1; i<32; i++){
        if(hscale[i] == 0){
            display.drawLine(graph[0]+(i*36), graph[1], graph[0]+(i*36), graph[1]+graph[3], GREENYELLOW);
        }
        else{
            display.drawLine(graph[0]+(i*36), graph[1], graph[0]+(i*36), graph[1]+graph[3], DARKGREY);
        }
    }
    
    // 温湿度グラフ
    for(x=0; x<(384-12); x++){
        // 湿度グラフ
        if(data[0][1][x] < 0) y = 0;
        else if(data[0][1][x] > 100) y = 100;
        else y = data[0][1][x];
        if(data[0][1][x+1] < 0) y2 = 0;
        else if(data[0][1][x+1] > 100) y2 = 100;
        else y2 = data[0][1][x+1];
        display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4, graph[0]+x*3+3, graph[1]+(100-y2)*4, MAROON);
        display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4, graph[0]+x*3+3+1, graph[1]+(100-y2)*4, MAROON);
        display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+1, graph[0]+x*3+3, graph[1]+(100-y2)*4+1, MAROON);
        display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4+1, graph[0]+x*3+3+1, graph[1]+(100-y2)*4+1, MAROON);
        // 温度グラフ
        if(data[0][0][x] < -10) y = -10;
        else if(data[0][0][x] > 50) y = 50;
        else y = data[0][0][x];
        if(data[0][0][x+1] < -10) y2 = -10;
        else if(data[0][0][x+1] > 50) y2 = 50;
        else y2 = data[0][0][x+1];
        display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8, graph[0]+x*3+3, graph[1]+(50-y2)*8, RED);
        display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8, graph[0]+x*3+3+1, graph[1]+(50-y2)*8, RED);
        display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+1, graph[0]+x*3+3, graph[1]+(50-y2)*8+1, RED);
        display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8+1, graph[0]+x*3+3+1, graph[1]+(50-y2)*8+1, RED);
    }
}


// グラフ追加分描画
// 最後の15分部分のみのグラフ描画（上書）
void graphdraw2(int pos)
{
    int x, y;
    int y2;
    
    int pos1 = 372 + pos - 1;
    int pos2 = 372 + pos;
    
    x = pos1;
    
    // 湿度グラフ
    if(data[0][1][pos1] < 0) y = 0;
    else if(data[0][1][pos1] > 100) y = 100;
    else y = data[0][1][pos1];
    if(data[0][1][pos2] < 0) y2 = 0;
    else if(data[0][1][pos2] > 100) y2 = 100;
    else y2 = data[0][1][pos2];
    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4, graph[0]+x*3+3, graph[1]+(100-y2)*4, MAROON);
    display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4, graph[0]+x*3+3+1, graph[1]+(100-y2)*4, MAROON);
    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+1, graph[0]+x*3+3, graph[1]+(100-y2)*4+1, MAROON);
    display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4+1, graph[0]+x*3+3+1, graph[1]+(100-y2)*4+1, MAROON);
    // 温度グラフ
    if(data[0][0][pos1] < -10) y = -10;
    else if(data[0][0][pos1] > 50) y = 50;
    else y = data[0][0][pos1];
    if(data[0][0][pos2] < -10) y2 = -10;
    else if(data[0][0][pos2] > 50) y2 = 50;
    else y2 = data[0][0][pos2];
    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8, graph[0]+x*3+3, graph[1]+(50-y2)*8, RED);
    display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8, graph[0]+x*3+3+1, graph[1]+(50-y2)*8, RED);
    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+1, graph[0]+x*3+3, graph[1]+(50-y2)*8+1, RED);
    display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8+1, graph[0]+x*3+3+1, graph[1]+(50-y2)*8+1, RED);
}


// 日時スケール表示
// n  : 表示位置 1～31（6時間刻み）
// dm : 0=日にち、1=時間
void dispscale(int n, int dm)
{
    display.setTextSize(1); 
    // フォント変更
    display.setFont(&fonts::lgfxJapanGothic_24);
    
    if(dm == 0){
        // 日表示
        display.setTextColor(BLACK, GREENYELLOW);
        display.setCursor(graph[0]+(n*36)-12, graph[1]-(24/2+2));
        display.printf("%02d", dscale[n]);  // 日
    }
    else{
        // 時表示
        display.setTextColor(LIGHTGREY, DARKGREY);
        display.setCursor(graph[0]+(n*36)-12, graph[1]-(24/2+2));
        display.printf("%02d", hscale[n]);  // 時
    }
    // フォント戻しておく
    display.setFont(&fonts::lgfxJapanGothic_40);
}


// 現時刻から3時間刻みを計算して表示に反映する
void timescale(int yr, int mn, int dt, int hh, int mm, int ss)
{
    struct tm t;
    struct tm *tm_info;
    int i;
    
    t.tm_year = yr - 1900;    // 年（1900 からの経過年）
    t.tm_mon  = mn - 1;       // 月（0 = 1月）
    t.tm_mday = dt;           // 日
    t.tm_hour = hh;           // 時
    t.tm_min  = mm;           // 分
    t.tm_sec  = ss;           // 秒
    
    time_t unixTime = mktime(&t);  // UNIX時間（秒）
    
    // 3時間刻みに丸め
    time_t tt = unixTime / (3*60*60);
    unixTime = (tt-31) * 3*60*60;  // グラフ左端時刻
    
    for(i=0; i<32; i++){
        tm_info = localtime(&unixTime);
        dscale[i] = tm_info->tm_mday;
        hscale[i] = tm_info->tm_hour;
        
        unixTime += (3*60*60);  // 3時間後
    }
    
    Serial.printf("\n DAY  : ");
    for(i=0; i<32; i++){
        Serial.printf("%02d ", dscale[i]);
    }
    Serial.printf("\n HOUR : ");
    for(i=0; i<32; i++){
        Serial.printf("%02d ", hscale[i]);
    }
    Serial.printf("\n");
}


// 自動的に時間を進めてサンプルデータを書き込むデモ
void loop()
{
    int i;
    unsigned long tmsnow = 0;
    
    // ボタン状態取得
    lgfx::touch_point_t tp[3];
    int nums = display.getTouchRaw(tp, 3);
    
    if(mode == 0){  // 時間表示中
        // 設定ボタンタッチあり
        checkbutton2(nums, tp[0].x, tp[0].y);
        
        tmsnow = millis();
        if((tmsnow - tms) >= 200 || tmsnow < tmm){
            // 先回から0.2秒経過 -> 時計更新
            tms += 200;
            // デモ用に0.2秒で1分加算
            addone2(&syear, &smonth, &sday, &shour, &smin, &ssec);
            
            if((tmsnow - tmm) >= 3600000 || tmsnow < tmm){
                // 先回から60分経過 or millis()オーバーフロー -> RTCで時刻補正
                if(tmsnow > tmm){
                    tmm += 3600000;
                }
                else{
                    tmm = tmsnow;  // オーバーフロー時
                }
                /* デモでRTC非使用
                if(rtc == 1){  // RTCあり
                    auto dt = M5.Rtc.getDateTime();
                    syear  = dt.date.year;
                    smonth = dt.date.month;
                    sday   = dt.date.date;
                    shour  = dt.time.hours;
                    smin   = dt.time.minutes;
                    ssec   = dt.time.seconds;
                }
                */
                // 時刻描画
                timedisp2(syear, smonth, sday, shour, smin, ssec, mode);
                
                Serial.println("!");
            }
            else{
                // 時刻描画
                timedisp2(syear, smonth, sday, shour, smin, ssec, mode);
                
                if(ssec == 0){
                    Serial.print(".");
                }
            }
            
            // 15分毎にデータ更新
            if((smin % 15) == 0){
                if((smin / 15) != datanew[0]){
                    // グラフ更新
                    if((shour % 3) == 0 && smin == 0){
                        tcount = 0;
                        // 横軸（時刻）3時間間隔で更新（左に3時間分移動）
                        for(i=0; i<(384-12); i++){
                            data[0][0][i] = data[0][0][i+12];
                            data[0][1][i] = data[0][1][i+12];
                        }
                        // データ最新値追加
                        data[0][0][372] = nowdata[0];
                        data[0][1][372] = nowdata[1];
                        datanew[0] = (smin / 15);
                        // グラフエリア描画
                        display.waitDisplay();
                        graphdraw();
                        display.display();
                        
                        Serial.printf("\n[0]tcount = %d\n", tcount);
                        
                    }
                    else{
                        if(tcount < 11){
                            tcount += 1;
                        }
                        // データ最新値追加
                        data[0][0][372+tcount] = nowdata[0];
                        data[0][1][372+tcount] = nowdata[1];
                        datanew[0] = (smin / 15);
                        // グラフエリア追加分描画
                        display.waitDisplay();
                        graphdraw2(tcount);
                        display.display();
                        
                        Serial.printf("\n[1]tcount = %d\n", tcount);
                        
                    }
                    
                    // デモデータ更新
                    nowdata[0] += 2;
                    if(nowdata[0] > 50) nowdata[0] = -10;
                    nowdata[1] += 1;
                    if(nowdata[1] > 100) nowdata[1] = 0;
                }
            }
        }
    }
    
    //delay(100); // 0.1秒待機
}


// 時刻加算（日時に1秒加算）デモ用
void addone2(int *yr, int *mn, int *dt, int *hh, int *mm, int *ss)
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
    
    unixTime += 60;
    
    tm_info = localtime(&unixTime);
    
    *yr = tm_info->tm_year + 1900;
    *mn = tm_info->tm_mon + 1;
    *dt = tm_info->tm_mday;
    *hh = tm_info->tm_hour;
    *mm = tm_info->tm_min;
    *ss = tm_info->tm_sec;
}


// 時刻描画
void timedisp2(int yr, int mn, int dt, int hh, int mm, int ss, int md)
{
    display.waitDisplay();
    
    //uint32_t colors = display.color888(0, 0, 0);
    int setupbtn[4] = {876, 10, 160, 100};
    
    display.fillRect(timepos[0], setupbtn[1], 1280-setupbtn[0]-10, setupbtn[3], DARKGREY);
    display.setTextSize(1); 
    display.setTextColor(GREEN, DARKGREY);
    display.setCursor(timepos[0], timepos[1]);
    display.printf("%04d/%02d/%02d %02d:%02d:%02d", 
        yr, mn, dt, hh, mm, ss
    );
    
    display.display();
}


// 設定ボタンエリア
void checkbutton2(int nums, int x, int y)
{
    int pos;
    
    int xx = y;  // 画面タッチは左下が原点（描画系は左上が原点）
    int yy = 719 - x;
    
    if(nums == 0){
        // リリース（ボタン確定）
        if(nowpos >= 0){
            // 横座標がボタン位置
            if(pos == 0){  // 設定開始
                // 設定画面切替
                Serial.print("Setup Button!");
                
                
                
            }
        }
        nowpos = -1;
    }
    else{
        // 押下中（ボタン判定）
        if(yy > setupbtn[1] && yy < setupbtn[1]+setupbtn[3]){
            // 縦座標がボタン位置
            if(xx > setupbtn[0] && xx < setupbtn[0]+setupbtn[2]){
                // 横座標がボタン位置
                nowpos = 0;
                nowx = xx;
                nowy = yy;
            }
        }
    }
}

