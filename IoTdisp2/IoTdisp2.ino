#include <arduino.h>
#include <M5GFX.h>
#include <M5Unified.h>
#include <time.h>
#include <SD.h>

// 動作は、2026年に設定後に開始する

// 現在、最大、最小温湿度はセンサー1番データを表示
// ※ Tab5は、Serialで出力しているとリセットがかかるので
// ※ 実際の実行時は「Serial」をコメントアウトする事

// LoRa受信データ構造（17バイト）
// IOT0x,XXXX,YYYY,Z
//  (1)  (2)  (3) (4)
// (1) ヘッダーASCII5バイト、固定5種、センサー番号
// (2) 温度ASCII4バイト、10倍の値（0243->24.3℃）
// (3) 湿度ASCII4バイト、10倍の値（0451->45.1％）
// (4) 人検知ASCII1バイト、0=無検知、1=検知中

// Unit Earth（土壌湿度センサー時）
// IOT1x,XXXX,YYYY,Z
//  (1)  (2)  (3) (4)
// (1) ヘッダーASCII5バイト、固定5種、センサー番号
// (2) 土壌湿度パーセントASCII4バイト、10倍の値（0243->24.3％）
// (3) 設定湿度閾値ASCII4バイト、10倍の値（0451->45.1％）
// (4) 土壌乾燥警告ASCII1バイト、0=問題なし、1=乾燥

// シリアル受信は、[CR(0D)+LF(0A)]付加して来る（19バイト）
// IOT01,XXXX,YYYY,Z(0D)(0A)

#define BUZZVOLMAX 255  // 人検出時のブザー音量大
#define BUZZVOLMID 128  // 人検出時のブザー音量中
#define BUZZVOLMIN 64   // 人検出時のブザー音量小

// ヘッダー定義
const char *header[10] = {
    "IOT01",
    "IOT02",
    "IOT03",
    "IOT04",
    "IOT05",
    "IOT11",
    "IOT12",
    "IOT13",
    "IOT14",
    "IOT15"
};

M5GFX display;
int rtc = 0;

int mode = -1;    // -1=グラフ表示 / 0=時計動作 1=年 2=月 3=日 4=時 5=分 6=秒
int nowpos = -1;  // 押されたボタン番号（0～3）放された時に処理
int nowx = -1;    // 画面上のタッチ位置
int nowy = -1;    // 画面上のタッチ位置

int syear, smonth, sday, shour, smin, ssec;  // 表示中の時刻
int dscale[32];  // 日スケール
int hscale[32];  // 時スケール
int tcount = -1;

int mandet = 0;  // 人検知
int buzzer = 0;  // ブザー
int sens[5] = { 0, 0, 0, 0, 0};  // センサーあり
int snew[5] = { 0, 0, 0, 0, 0};  // 新センサーデータ
int earth[5] = { 0, 0, 0, 0, 0};  // 土壌湿度センサー時「1」

unsigned long tms = 0;  // システム経過時間（msec）1秒計測用
unsigned long tmm = 0;  // システム経過時間（msec）60分計測用

// 設定ボタン 1280-64-200-20-200, 10, 200, 100
int setupbtn[4] = {796, 10, 200, 100};
// 人検知表示 1280-64-200, 10, 200, 100
int mandisp[4] = {1016, 10, 200, 100};
// 時刻表示 64, 10+40/2（文字高40）
int timepos[2] = {64, 30};
// SD表示 796-196, 30
int sdpos[2] = {550, 30};
int sdcount = 0;
// 各センサー現在表示 64, 80+24/2（文字高24）
int senspos[4] = {20, 92};
// グラフ表示（グラフ線幅分+2）
int graph[4] = {64, 160, 1152+2, 480};
// 温湿度表示
int temppos[2] = {64, 690};

// 温湿度データ（10倍）4日分×24時間×4回(15分刻み）＝384
int data[5][2][384];
int datanew[5] = {-1, -1, -1, -1, -1};
int dataok[5][384];
// SD保存
int datanum[5] = {0,0,0,0,0};  // 384貯まったらSD保存
char ssd[5][16000];
int sdok = 0;

// 最新の取得データ
int nowdata[5][3] = {0, 0, 0};

// 温湿度の最高、最低（グラフ表示中で）
int nowtemp = 0;
int nowhygn = 0;
int lasttemp = 0;
int lasthygn = 0;
int tempmax = -100;
int tempmin = 500;
int hygromax = 0;
int hygromin = 1000;

// 各センサー現在
int nowsens[5][2] = {{0,0},{0,0},{0,0},{0,0},{0,0}};

// 人検知（センサー番号セット）
int mdetect = -1;
int sound = 0;
int vol = 0;  // 音量 0=大、1=中、2=小

void setup()
{
    int i;
    int j;
    
    for(j=0; j<5; j++){
        for(i=0; i<384; i++){
            data[j][0][i] = 0;
            data[j][1][i] = 0;
            dataok[j][i] = 0;
        }
    }
    
    auto cfg = M5.config();
    M5.begin(cfg);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("Start Disp...");
    
    Serial2.begin(115200, SERIAL_8N1, 53, 54);
    
    // SDカード
    if(!SD.begin(GPIO_NUM_42, SPI, 25000000)){
        Serial.println("SD init failed");
        sdok = 0;
    }
    else{
        Serial.println("SD inited.");
        sdok = 1;
    }
    
    if(!M5.Rtc.isEnabled()){
        Serial.println("ERROR : RTC not Found!");
    }
    else{
        //M5.Rtc.setDateTime( { { 2020, 1, 1 }, { 13, 0, 0 } } );
        syear  = 2020;
        smonth = 1;
        sday   = 1;
        shour  = 13;
        smin   = 0;
        ssec   = 0;
        M5.Rtc.setDateTime({{syear, smonth, sday}, {shour, smin, ssec}});
        
        Serial.println("Set to RTC = 2020/1/1 13:00:00");
        rtc = 1;
    }
    
    M5.Lcd.setTextDatum(4);  // 0（左上）～8（右下）
    M5.Lcd.setRotation(1);   // 90度回転（0〜3）
    
    display = M5.Display;
    
    // 表示セットアップ
    display.init();
    
    graphdisp(0);
    
    if(sdok == 0){
        display.setFont(&fonts::lgfxJapanGothic_40);
        display.setTextSize(1); 
        display.setTextColor(BLACK, DARKGREY);
        display.setCursor(sdpos[0], sdpos[1]);
        display.printf("Non SD");
    }
    else{
        display.setFont(&fonts::lgfxJapanGothic_40);
        display.setTextSize(1); 
        display.setTextColor(CYAN, DARKGREY);
        display.setCursor(sdpos[0], sdpos[1]);
        display.printf("SD OK %d", sdcount);
    }
    
    tms = millis();
    tmm = millis();
}


// mm：呼出元判定（0=初期化、1=セットアップ終了）
void graphdisp(int mm)
{
    char ss[200];
    int nowt, maxt, mint;
    
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
    display.setFont(&fonts::lgfxJapanGothic_40);
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
    
    if(mm == 0){
        // 温湿度表示
        display.fillRect(0, 660, 1280, 60, DARKGREY);
        display.setFont(&fonts::lgfxJapanGothic_40);
        display.setTextColor(CYAN, DARKGREY);
        display.setTextSize(1); 
        display.setCursor(temppos[0], temppos[1]);
        display.printf("現在 16.8℃ 55％ : 最高 20.5℃ 60％ / 最低 10.6℃ 35％");
        
        // 全センサー現在表示
        sensdisp();
    }
    else{
        display.fillRect(0, 660, 1280, 60, DARKGREY);
        display.setFont(&fonts::lgfxJapanGothic_40);
        display.setTextSize(1);
        display.setTextColor(CYAN, DARKGREY);
        display.setCursor(temppos[0], temppos[1]);
        if(nowtemp >= 0) nowt = nowtemp;
        else             nowt = nowtemp * (-1);
        if(tempmax >= 0) maxt = tempmax;
        else             maxt = tempmax * (-1);
        if(tempmin >= 0) mint = tempmin;
        else             mint = tempmin * (-1);
        sprintf(ss, "現在 %d.%d℃ %d％ : 最高 %d.%d℃ %d％ / 最低 %d.%d℃ %d％　",
        nowtemp/10, nowt%10, nowhygn/10,
        tempmax/10, maxt%10, hygromax/10,
        tempmin/10, mint%10, hygromin/10);
        display.printf(ss);
        display.display();
        
    }
    
    // 設定ボタン
    display.setFont(&fonts::lgfxJapanGothic_40);
    display.setTextSize(1); 
    display.setTextColor(BLACK);
    display.fillRect(setupbtn[0], setupbtn[1], setupbtn[2], setupbtn[3], DARKGREEN);
    display.setCursor(setupbtn[0]+setupbtn[2]/2-display.textWidth("設 定")/2, setupbtn[1]+setupbtn[3]/2);
    display.printf("設 定");
    
    // 人検知表示
    display.setFont(&fonts::lgfxJapanGothic_40);
    display.setTextSize(1); 
    if(mandet == 0){
        display.setTextColor(LIGHTGREY);
        display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], BLACK);
        display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth("検知オフ")/2, mandisp[1]+mandisp[3]/2);
        display.printf("検知オフ");
    }
    else{
        display.setTextColor(BLACK);
        display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], DARKCYAN);
        display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth("検知オン")/2, mandisp[1]+mandisp[3]/2);
        display.printf("検知オン");
    }
    
    display.display();
    
}


// 全センサー現在表示
void sensdisp()
{
    char sss[50];
    int nowt, nowh;
    int i;
    int x;
    
    // 各センサー現在表示 64, 80+24/2（文字高24）
    display.setFont(&fonts::lgfxJapanGothic_24);
    display.setTextSize(1);
    x = senspos[0];
    display.fillRect(senspos[0], 80, setupbtn[0]-senspos[0], 24, DARKGREY);
    for(i=0; i<5; i++){
        if(earth[i] == 0){
            switch(i){
                case 0  : display.setTextColor(RED);     break;
                case 1  : display.setTextColor(BLUE);    break;
                case 2  : display.setTextColor(GREEN);   break;
                case 3  : display.setTextColor(ORANGE);  break;
                case 4  : display.setTextColor(MAGENTA); break;
                default : display.setTextColor(CYAN);    break;
            }
        }
        else{  // 土壌湿度センサー
            switch(i){
                case 0  : display.setTextColor(RED, DARKCYAN);     break;
                case 1  : display.setTextColor(BLUE, DARKCYAN);    break;
                case 2  : display.setTextColor(GREEN, DARKCYAN);   break;
                case 3  : display.setTextColor(ORANGE, DARKCYAN);  break;
                case 4  : display.setTextColor(MAGENTA, DARKCYAN); break;
                default : display.setTextColor(CYAN, DARKCYAN);    break;
            }
        }
        if(nowsens[i][0] < 0) nowt = (nowsens[i][0] * (-1)) % 10;
        else                  nowt = nowsens[i][0] % 10;
        if(nowsens[i][1] < 0) nowh = (nowsens[i][1] * (-1)) % 10;
        else                  nowh = nowsens[i][1] % 10;
        if(earth[i] == 0){
            sprintf(sss, "%d.%d℃,%d％ ", nowsens[i][0]/10, nowt, nowsens[i][1]/10);
        }
        else{  // 土壌湿度センサー
            sprintf(sss, "%d.%d％ ", nowsens[i][0]/10, nowt);
        }
        display.setCursor(x, senspos[1]);
        x +=display.textWidth(sss);
        display.printf(sss);
    }
    display.display();
}


// グラフ再描画
// グラフエリア全ての更新（3時間分左シフト）
void graphdraw()
{
    int x, y;
    int y2;
    int i;
    int j;
    int color1, color2;
    
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
    display.setFont(&fonts::lgfxJapanGothic_40);
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
    display.setTextSize(1); 
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
    for(j=4; j>=0; j--){
        if(sens[j] != 0){
            switch(j){
                case 0  : color1 = RED;     color2 = MAROON;    break;
                case 1  : color1 = BLUE;    color2 = NAVY;      break;
                case 2  : color1 = GREEN;   color2 = DARKGREEN; break;
                case 3  : color1 = ORANGE;  color2 = OLIVE;     break;
                case 4  : color1 = MAGENTA; color2 = PURPLE;    break;
                default : color1 = CYAN;    color2 = DARKCYAN;  break;
            }
            for(x=0; x<(384-12); x++){
                if(dataok[j][x] != 0){
                    // 湿度グラフ
                    if(data[j][1][x] < 0) y = 0;
                    else if(data[j][1][x] > 1000) y = 100;
                    else y = data[j][1][x] / 10;
                    if(data[j][1][x+1] < 0) y2 = 0;
                    else if(data[j][1][x+1] > 1000) y2 = 100;
                    else y2 = data[j][1][x+1] / 10;
                    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4, graph[0]+x*3+3, graph[1]+(100-y2)*4, color2);
                    display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4, graph[0]+x*3+3+1, graph[1]+(100-y2)*4, color2);
                    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4-1, graph[0]+x*3+3, graph[1]+(100-y2)*4-1, color2);
                    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+1, graph[0]+x*3+3, graph[1]+(100-y2)*4+1, color2);
                    display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+2, graph[0]+x*3+3, graph[1]+(100-y2)*4+2, color2);
                    display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4+1, graph[0]+x*3+3+1, graph[1]+(100-y2)*4+1, color2);
                    // 温度グラフ
                    if(data[j][0][x] < -100) y = -10;
                    else if(data[j][0][x] > 500) y = 50;
                    else y = data[j][0][x] / 10;
                    if(data[j][0][x+1] < -100) y2 = -10;
                    else if(data[j][0][x+1] > 500) y2 = 50;
                    else y2 = data[j][0][x+1] / 10;
                    if(earth[j] != 0){  // 土壌湿度センサー
                        // 温度スケールに表示する為に1/2倍にする
                        y /= 2;
                        y2 /= 2;
                    }
                    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8, graph[0]+x*3+3, graph[1]+(50-y2)*8, color1);
                    display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8, graph[0]+x*3+3+1, graph[1]+(50-y2)*8, color1);
                    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8-1, graph[0]+x*3+3, graph[1]+(50-y2)*8-1, color1);
                    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+1, graph[0]+x*3+3, graph[1]+(50-y2)*8+1, color1);
                    display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+2, graph[0]+x*3+3, graph[1]+(50-y2)*8+2, color1);
                    display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8+1, graph[0]+x*3+3+1, graph[1]+(50-y2)*8+1, color1);
                }
            }
        }
    }
}


// グラフ追加分描画
// 最後の15分部分のみのグラフ描画（上書）
void graphdraw2(int pos)
{
    int x, y;
    int y2;
    int j;
    int color1, color2;
    
    int pos1 = 372 + pos - 1;
    int pos2 = 372 + pos;
    
    x = pos1;
    
    for(j=4; j>=0; j--){
        if(sens[j] != 0){
            switch(j){
                case 0  : color1 = RED;     color2 = MAROON;    break;
                case 1  : color1 = BLUE;    color2 = NAVY;      break;
                case 2  : color1 = GREEN;   color2 = DARKGREEN; break;
                case 3  : color1 = ORANGE;  color2 = OLIVE;     break;
                case 4  : color1 = MAGENTA; color2 = PURPLE;    break;
                default : color1 = CYAN;    color2 = DARKCYAN;  break;
            }
            if(dataok[j][x] != 0){
                // 湿度グラフ
                if(data[j][1][pos1] < 0) y = 0;
                else if(data[j][1][pos1] > 1000) y = 100;
                else y = data[j][1][pos1] / 10;
                if(data[j][1][pos2] < 0) y2 = 0;
                else if(data[j][1][pos2] > 1000) y2 = 100;
                else y2 = data[j][1][pos2] / 10;
                display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4, graph[0]+x*3+3, graph[1]+(100-y2)*4, color2);
                display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4, graph[0]+x*3+3+1, graph[1]+(100-y2)*4, color2);
                display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4-1, graph[0]+x*3+3, graph[1]+(100-y2)*4-1, color2);
                display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+1, graph[0]+x*3+3, graph[1]+(100-y2)*4+1, color2);
                display.drawLine(graph[0]+x*3, graph[1]+(100-y)*4+2, graph[0]+x*3+3, graph[1]+(100-y2)*4+2, color2);
                display.drawLine(graph[0]+x*3+1, graph[1]+(100-y)*4+1, graph[0]+x*3+3+1, graph[1]+(100-y2)*4+1, color2);
                // 温度グラフ
                if(data[j][0][pos1] < -100) y = -10;
                else if(data[j][0][pos1] > 500) y = 50;
                else y = data[j][0][pos1] / 10;
                if(data[j][0][pos2] < -100) y2 = -10;
                else if(data[j][0][pos2] > 500) y2 = 50;
                else y2 = data[j][0][pos2] / 10;
                if(earth[j] != 0){  // 土壌湿度センサー
                    // 温度スケールに表示する為に1/2倍にする
                    y /= 2;
                    y2 /= 2;
                }
                display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8, graph[0]+x*3+3, graph[1]+(50-y2)*8, color1);
                display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8, graph[0]+x*3+3+1, graph[1]+(50-y2)*8, color1);
                display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8-1, graph[0]+x*3+3, graph[1]+(50-y2)*8-1, color1);
                display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+1, graph[0]+x*3+3, graph[1]+(50-y2)*8+1, color1);
                display.drawLine(graph[0]+x*3, graph[1]+(50-y)*8+2, graph[0]+x*3+3, graph[1]+(50-y2)*8+2, color1);
                display.drawLine(graph[0]+x*3+1, graph[1]+(50-y)*8+1, graph[0]+x*3+3+1, graph[1]+(50-y2)*8+1, color1);
            }
        }
    }
}


// 日時スケール表示
// n  : 表示位置 1～31（6時間刻み）
// dm : 0=日にち、1=時間
void dispscale(int n, int dm)
{
    // フォント変更
    display.setFont(&fonts::lgfxJapanGothic_24);
    display.setTextSize(1); 
    
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
    
    /*
    Serial.printf("\n DAY  : ");
    for(i=0; i<32; i++){
        Serial.printf("%02d ", dscale[i]);
    }
    Serial.printf("\n HOUR : ");
    for(i=0; i<32; i++){
        Serial.printf("%02d ", hscale[i]);
    }
    Serial.printf("\n");
    */
}


void loop()
{
    int i;
    int num = -1;
    int ear = 0;   // 土壌湿度センサー
    String s;
    int tmp = 1000;
    int hyg = 1000;
    int man = 1000;
    int nowt;
    char ss[200];
    int color;
    
    // Arduino Nesso N1からのシリアル通信受信
    if(Serial2.available()){
        String text = Serial2.readStringUntil(0x0a);
        // 受信文字列長が0以上で「IOT」から始まる文字列の時だけ処理する
        if(text.length() > 0 && text.startsWith("IOT")){
            text.trim();  // スペース削除
            //text.replace("*","");
            
//          Serial.println(text);
            if(text.length() != 17){
                //Serial.println("Serial Data Length Error : ");
                //Serial.printf("%d", text.length());
                //Serial.printf("B != 17\n");
            }
            else{
                if(text.startsWith(header[0])){      num = 0; earth[0] = 0; ear = 0;}
                else if(text.startsWith(header[1])){ num = 1; earth[1] = 0; ear = 0;}
                else if(text.startsWith(header[2])){ num = 2; earth[2] = 0; ear = 0;}
                else if(text.startsWith(header[3])){ num = 3; earth[3] = 0; ear = 0;}
                else if(text.startsWith(header[4])){ num = 4; earth[4] = 0; ear = 0;}
                else if(text.startsWith(header[5])){ num = 0; earth[0] = 1; ear = 1;}
                else if(text.startsWith(header[6])){ num = 1; earth[1] = 1; ear = 1;}
                else if(text.startsWith(header[7])){ num = 2; earth[2] = 1; ear = 1;}
                else if(text.startsWith(header[8])){ num = 3; earth[3] = 1; ear = 1;}
                else if(text.startsWith(header[9])){ num = 4; earth[4] = 1; ear = 1;}
                
                if(num >= 0){
                    // IOT01,XXXX,YYYY,Z
                    // データを格納 
                    s = text.substring(6, 10);
                    tmp = s.toInt();
                    s = text.substring(11, 15);
                    hyg = s.toInt();
                    s = text.substring(16, 17);
                    man = s.toInt();
                    if(tmp <= 1000 && hyg <= 1000 && man < 1000){
                        nowdata[num][0] = tmp; // 温度（×10倍）
                        nowdata[num][1] = hyg; // 湿度（×10倍）
                        
                        if(mode < 0){
                            if(man == 1){
                                nowdata[num][2] = 1; // 検知（0、1）
                                if(mandet != 0){
                                    if(mdetect < 0){
                                        // 検出
                                        mdetect = num;
                                        // 検知！表示
                                        display.setTextColor(BLACK);
                                        if(ear != 0){
                                            color = CYAN;
                                        }
                                        else{
                                            switch(num){
                                                case 0  : color = RED;     break;
                                                case 1  : color = BLUE;    break;
                                                case 2  : color = GREEN;   break;
                                                case 3  : color = ORANGE;  break;
                                                case 4  : color = MAGENTA; break;
                                                default : color = CYAN;    break;
                                            }
                                        }
                                        display.setFont(&fonts::lgfxJapanGothic_40);
                                        display.setTextSize(1); 
                                        display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], color);
                                        if(ear == 0){
                                            sprintf(ss, "人検知%d", num+1);
                                        }
                                        else{
                                            sprintf(ss, "乾燥%d", num+1);
                                        }
                                        display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth(ss)/2, mandisp[1]+mandisp[3]/2);
                                        display.printf(ss);
                                        display.display();
                                        // ブザー発音
                                        if(buzzer != 0){
                                            sound = 1;
                                            // 音量
                                            if(vol == 0)      M5.Speaker.setVolume(BUZZVOLMAX);
                                            else if(vol == 1) M5.Speaker.setVolume(BUZZVOLMID);
                                            else              M5.Speaker.setVolume(BUZZVOLMIN);
                                            M5.Speaker.tone(1000,700);
                                        }
                                        else{
                                            mdetect += 10;
                                        }
                                    }
                                }
                            }
                        }
                        
                        if(mode < 0){
                            if(nowsens[num][0] != tmp || nowsens[num][1] != hyg){
                                nowsens[num][0] = tmp; // 温度（×10倍）
                                nowsens[num][1] = hyg; // 湿度（×10倍）
                                // 全センサー現在表示
                                sensdisp();
                            }
                        }
                        
                        //Serial.println(nowdata[num][0]);
                        //Serial.println(nowdata[num][1]);
                        //Serial.println(nowdata[num][2]);
                    }
                    
                    // 1回でも受信したら0データとして表示
                    if(mode < 0){  // グラフ表示中
                        if(sens[num] == 0){
                            sens[num] = 1;
                            for(i=0; i<384; i++){
                                dataok[num][i] = 1;  // グラフ描画（1or2）
                            }
                            
                            display.waitDisplay();
                            graphdraw();
                            display.display();
                            
                            if(num == 0){
                                // 最大、最低温度設定（センサーNo.1）
                                nowtemp = nowdata[0][0];
                                nowhygn = nowdata[0][1];
                                if(lasttemp != nowtemp || lasthygn != nowhygn){
                                    lasttemp = nowtemp;
                                    lasthygn = nowhygn;
                                }
                                if(nowtemp >= 0) nowt = nowtemp;
                                else             nowt = nowtemp * (-1);
                                display.fillRect(0, 660, 1280, 60, DARKGREY);
                                display.setFont(&fonts::lgfxJapanGothic_40);
                                display.setTextSize(1);
                                display.setTextColor(CYAN, DARKGREY);
                                display.setCursor(temppos[0], temppos[1]);
                                if(ear == 0){
                                    sprintf(ss, "現在 %d.%d℃ %d％ : 最高 %d.%d℃ %d％ / 最低 %d.%d℃ %d％　",
                                    nowtemp/10, nowt%10, nowhygn/10,
                                    nowtemp/10, nowt%10, nowhygn/10,
                                    nowtemp/10, nowt%10, nowhygn/10);
                                }
                                else{
                                    sprintf(ss, "土壌湿度 %d.%d％ : 最高 %d.%d％ / 最低 %d.%d％　　　",
                                    nowtemp/10, nowt%10,
                                    nowtemp/10, nowt%10,
                                    nowtemp/10, nowt%10);
                                }
                                display.printf(ss);
                                display.display();
                                tempmax = nowtemp;
                                tempmin = nowtemp;
                                hygromax = nowhygn;
                                hygromin = nowhygn;
                            }
                        }
                    }
                    
                    snew[num] = 1;
                }
            }
            
            //display.waitDisplay();
            //display.println(text); //.c_str());
            //display.display();
        }
    }
    
    // 受信データ処理（15分刻み）
    checkdata();
    
    if(mode < 0){  // グラフ表示中
        loopdisp();
    }
    else{
        loopsetup();
    }
    
    // スピーカーを音程上下させる為の処理
    // 1回の発音を1秒にして、終わっていたら音程を切り替えて
    // 再発音をくり返す
    if(mdetect >= 0 && mdetect < 10 && buzzer != 0){
        if(!M5.Speaker.isPlaying()){
            if(sound != 0){
                //M5.Speaker.setVolume(BUZZVOL / 2);  // 音量/2
                M5.Speaker.tone(1800,700);
                sound = 0;
            }
            else{
                //M5.Speaker.setVolume(BUZZVOL);  // 音量
                M5.Speaker.tone(1000,700);
                sound = 1;
            }
        }
    }
}


// グラフ表示時のloop()処理
void loopdisp()
{
    // ボタン状態取得
    lgfx::touch_point_t tp[3];
    int nums = display.getTouchRaw(tp, 3);
    
    // 設定ボタンタッチあり
    checkbutton2(nums, tp[0].x, tp[0].y);
    
}


// 受信データ処理（15分刻み）
void checkdata()
{
    int i;
    int j;
    unsigned long tmsnow = 0;
    int newmaxmin = 0;
    int nowt, maxt, mint;
    int draw = 0;
    char ss[200];
    int x1, x2;
    
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
            if(mode < 0){
                timedisp2(syear, smonth, sday, shour, smin, ssec, mode);
            }
            else{
                timedisp(syear, smonth, sday, shour, smin, ssec, mode);
            }
            
            //Serial.println("!");
        }
        else{
            // 時刻描画
            if(mode < 0){
                timedisp2(syear, smonth, sday, shour, smin, ssec, mode);
            }
            else{
                timedisp(syear, smonth, sday, shour, smin, ssec, mode);
            }
            
            if(ssec == 0){
                //Serial.print(".");
            }
        }
        
        #if 1  // @@@
        // 15分毎にデータ更新
        if((smin % 15) == 0){
        #else
        // 15秒毎にデータ更新 
        if((ssec % 15) == 0){
        #endif
            for(j=0; j<5; j++){
                if(sens[j] != 0 && snew[j] != 0){
                    //Serial.println("Data!");
                    #if 1  // @@@
                    // 前の描画から15分経過した
                    if((smin / 15) != datanew[j]){
                        if(j == 0){
                            // 温湿度の最大、最小確認
                            for(i=0; i<384; i++){
                                if(dataok[j][i] == 2){  // 受信データあり＝2
                                    if(data[j][0][i] > tempmax){
                                        tempmax = data[j][0][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][0][i] < tempmin){
                                        tempmin = data[j][0][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][1][i] > hygromax){
                                        hygromax = data[j][1][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][1][i] < hygromin){
                                        hygromin = data[j][1][i];
                                        newmaxmin = 1;
                                    }
                                }
                            }
                            
                            if(j == 0){  // センサーNo.1
                                nowtemp = nowdata[0][0];
                                nowhygn = nowdata[0][1];
                                if(lasttemp != nowtemp || lasthygn != nowhygn){
                                    lasttemp = nowtemp;
                                    lasthygn = nowhygn;
                                    newmaxmin = 1;
                                }
                            }
                        }
                        
                        // グラフ更新
                        if((shour % 3) == 0 && smin == 0){
                            // 3時間後との処理（グラフシフト）
                    #else
                    // 前の描画から15秒経過した
                    if((ssec / 15) != datanew[j]){
                        //Serial.println("New Data!");
                        if(j == 0){  // センサーNo.1
                            // 温湿度の最大、最小確認
                            for(i=0; i<384; i++){
                                if(dataok[j][i] == 2){  // 受信データあり＝2
                                    if(data[j][0][i] > tempmax){
                                        tempmax = data[j][0][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][0][i] < tempmin){
                                        tempmin = data[j][0][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][1][i] > hygromax){
                                        hygromax = data[j][1][i];
                                        newmaxmin = 1;
                                    }
                                    if(data[j][1][i] < hygromin){
                                        hygromin = data[j][1][i];
                                        newmaxmin = 1;
                                    }
                                }
                            }
                            
                            if(j == 0){  // センサーNo.1
                                nowtemp = nowdata[0][0];
                                nowhygn = nowdata[0][1];
                                if(lasttemp != nowtemp || lasthygn != nowhygn){
                                    lasttemp = nowtemp;
                                    lasthygn = nowhygn;
                                    newmaxmin = 1;
                                }
                            }
                        }
                        
                        // グラフ更新
                        if((smin % 3) == 0 && ssec == 0){
                            // 3分後との処理（グラフシフト）
                    #endif
                            if(j == 0){  // センサーNo.1
                                tcount = 0;
                                //Serial.printf("[shift]tcount = %d\n", tcount);
                                
                                // グラフ描画
                                draw = 1;
                            }
                            
                            // 横軸（時刻）3時間間隔で更新（左に3時間分移動）
                            for(i=0; i<(384-12); i++){
                                data[j][0][i] = data[j][0][i+12];
                                data[j][1][i] = data[j][1][i+12];
                            }
                            // データ最新値追加
                            data[j][0][372] = nowdata[j][0];
                            data[j][1][372] = nowdata[j][1];
                            #if 1  // @@@
                            datanew[j] = (smin / 15);
                            #else
                            datanew[j] = (ssec / 15);
                            #endif
                            
                            dataok[j][372] = 2;
                            for(i=373; i<384; i++){
                                dataok[j][i] = 1;
                            }
                            
                            // SD保存
                            sdw(j, nowdata[j][0], nowdata[j][1], nowdata[j][2]);
                            
                            if(j == 0){  // センサーNo.1
                                nowtemp = nowdata[0][0];
                                nowhygn = nowdata[0][1];
                                if(lasttemp != nowtemp || lasthygn != nowhygn){
                                    lasttemp = nowtemp;
                                    lasthygn = nowhygn;
                                    newmaxmin = 1;
                                }
                            }
                            
                            nowdata[j][0] = 0;
                            nowdata[j][1] = 0;
                            nowdata[j][2] = 0;
                            snew[j] = 0;
                        }
                        else{
                            // 15分毎の処理
                            if(j == 0){  // センサーNo.1
                                if(tcount < 0){
                                    // 始めにdata[]への書込開始位置を設定する
                                    // 3時間の12刻み（15分間隔）
                                    x1 = shour % 3;  // 0～2
                                    x2 = smin / 15;  // 0～3
                                    tcount = x1 * 4 + x2;
                                    //Serial.printf("[set]tcount = %d\n", tcount);
                                }
                                else if(tcount < 11){
                                    tcount += 1;
                                    //Serial.printf("[add]tcount = %d\n", tcount);
                                }
                                
                                // グラフ描画
                                draw = 2;
                            }
                            
                            // データ最新値追加
                            data[j][0][372+tcount] = nowdata[j][0];
                            data[j][1][372+tcount] = nowdata[j][1];
                            #if 1  // @@@
                            datanew[j] = (smin / 15);
                            #else
                            datanew[j] = (ssec / 15);
                            #endif
                            dataok[j][372+tcount] = 2;
                            
                            // SD保存
                            sdw(j, nowdata[j][0], nowdata[j][1], nowdata[j][2]);
                            
                            if(j == 0){  // センサーNo.1
                                nowtemp = nowdata[0][0];
                                nowhygn = nowdata[0][1];
                                if(lasttemp != nowtemp || lasthygn != nowhygn){
                                    lasttemp = nowtemp;
                                    lasthygn = nowhygn;
                                    newmaxmin = 1;
                                }
                            }
                            
                            nowdata[j][0] = 0;
                            nowdata[j][1] = 0;
                            nowdata[j][2] = 0;
                            snew[j] = 0;
                        }
                    }
                }
                
                if(mode < 0){  // グラフ表示中
                    // 最大最小更新（センサーNo.1）
                    if(newmaxmin != 0){
                        display.fillRect(0, 660, 1280, 60, DARKGREY);
                        display.setFont(&fonts::lgfxJapanGothic_40);
                        display.setTextSize(1);
                        display.setTextColor(CYAN, DARKGREY);
                        display.setCursor(temppos[0], temppos[1]);
                        if(nowtemp >= 0) nowt = nowtemp;
                        else             nowt = nowtemp * (-1);
                        if(tempmax >= 0) maxt = tempmax;
                        else             maxt = tempmax * (-1);
                        if(tempmin >= 0) mint = tempmin;
                        else             mint = tempmin * (-1);
                        if(earth[0] == 0){
                            sprintf(ss, "現在 %d.%d℃ %d％ : 最高 %d.%d℃ %d％ / 最低 %d.%d℃ %d％　",
                            nowtemp/10, nowt%10, nowhygn/10,
                            tempmax/10, maxt%10, hygromax/10,
                            tempmin/10, mint%10, hygromin/10);
                        }
                        else{
                            sprintf(ss, "土壌湿度 %d.%d％ : 最高 %d.%d％ / 最低 %d.%d％　　　",
                            nowtemp/10, nowt%10,
                            tempmax/10, maxt%10,
                            tempmin/10, mint%10);
                        }
                        display.printf(ss);
                        display.display();
                    }
                }
            }
            
            // グラフ描画
            // 5つのセンサーデータの更新が終わったここで表示更新する
            if(draw > 0){
                redraw(draw);
                draw = 0;
            }
            
        }
    }
}


// SD保存
//#define SAVEINTERVAL 96  // 96データ＝4×24h（15刻みで1日分）
#define SAVEINTERVAL 12  // 12データ＝3時間ごと @@@
void sdw(int sensnum, int nowdt0, int nowdt1, int nowdt2)
{
    char fname[40];
    char ss[200];
    int i;
    
    if(sdok != 0 && syear >= 2026 && mode < 0){
        // SD保存
        sprintf(ss, "%04d/%02d/%02d,%02d:%02d:%02d,%04d,%04d,%d", 
        syear, smonth, sday, shour, smin, ssec, nowdt0, nowdt1, nowdt2);
        ss[31] = 0x0D;
        ss[32] = 0x0A;
        ss[33] = 0;
        for(i=0; i<33; i++){
            ssd[sensnum][datanum[sensnum]*33+i] = ss[i];
        }
        datanum[sensnum] += 1;
        // 384データ＝4×24h×4日（15刻みで4日分）になったら
        if(datanum[sensnum] >= SAVEINTERVAL){
            sprintf(fname, "/%04d%02d%02d%02d%02d%02d-%d.csv", 
            syear, smonth, sday, shour, smin, ssec, sensnum+1);
            sdsave(fname, ssd[sensnum], 33*SAVEINTERVAL);
            datanum[sensnum] = 0;
        }
    }
}


// グラフ描画
// draw : 0=全画面シフト＆再描画、1=最後の3時間分のみ再描画
void redraw(int draw)
{
    if(mode < 0){  // グラフ表示中
        if(draw == 1){
            // グラフエリア描画
            display.waitDisplay();
            graphdraw();
            display.display();
        }
        else{
            // グラフエリア追加分描画
            display.waitDisplay();
            graphdraw2(tcount);
            display.display();
        }
    }
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
void timedisp2(int yr, int mn, int dt, int hh, int mm, int ss, int md)
{
    display.waitDisplay();
    
    //uint32_t colors = display.color888(0, 0, 0);
    int setupbtn[4] = {876, 10, 160, 100};
    
    display.fillRect(timepos[0], 10, sdpos[0]-timepos[0]-10, 40, DARKGREY);
    display.setFont(&fonts::lgfxJapanGothic_40);
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
    int xx = y;  // 画面タッチは左下が原点（描画系は左上が原点）
    int yy = 719 - x;
    
    if(nums == 0){
        // リリース（ボタン確定）
        if(nowpos >= 0){
            // 横座標がボタン位置
            if(nowpos == 0){  // 設定開始
                // 設定画面切替
                //Serial.println("Setup Button!");
                // セットアップ画面表示
                setupdisp();
                mode = 0;  // セットアップモード（時計表示）
            }
            else if(nowpos == 1){  // 検知ブザー停止
                //Serial.println("Mandet Button!");
                if(mdetect >= 0 && mdetect < 10){
                    // 1回目の押しで、ブザーが鳴っていれば停止
                    // ブザー無しならそのまま表示を戻し検出可能状態に
                    if(buzzer != 0){
                        // ブザーのみ停止、表示はそのまま
                        mdetect += 10;
                        M5.Speaker.stop();
                    }
                    else{
                        // 表示を戻す（無検出状態に）
                        display.setFont(&fonts::lgfxJapanGothic_40);
                        display.setTextSize(1); 
                        display.setTextColor(BLACK);
                        display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], DARKCYAN);
                        display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth("検知オン")/2, mandisp[1]+mandisp[3]/2);
                        display.printf("検知オン");
                        mdetect = -1;
                    }
                }
                else if(mdetect >= 10){
                    // 2回目の押しで表示を戻す（無検出状態に）
                    display.setFont(&fonts::lgfxJapanGothic_40);
                    display.setTextSize(1); 
                    display.setTextColor(BLACK);
                    display.fillRect(mandisp[0], mandisp[1], mandisp[2], mandisp[3], DARKCYAN);
                    display.setCursor(mandisp[0]+mandisp[2]/2-display.textWidth("検知オン")/2, mandisp[1]+mandisp[3]/2);
                    display.printf("検知オン");
                    mdetect = -1;
                }
            }
            else{
                //Serial.println("??? Button!");
            }
        }
        nowpos = -1;
    }
    else{
        // 押下中（ボタン判定）
        if(yy > setupbtn[1] && yy < setupbtn[1]+setupbtn[3]){
            if(xx > setupbtn[0] && xx < setupbtn[0]+setupbtn[2]){
                // 設定ボタン
                nowpos = 0;
                nowx = xx;
                nowy = yy;
            }
            else if(xx > mandisp[0] && xx < mandisp[0]+mandisp[2]){
                // 検知
                nowpos = 1;
                nowx = xx;
                nowy = yy;
            }
        }
    }
}


// SD SAVE
void sdsave(char *filename, char *ss, int size)
{
    File fw;
    size_t written;
    
    // SDカード読み書き
    if(sdok != 0){
        //Serial.print("FILE NAME : ");
        //Serial.println(filename);
        fw = SD.open(filename, FILE_WRITE);
        if(fw){
            written = fw.write((uint8_t*)ss, size);
            fw.close();
            //Serial.println("FILE SAVED (SD)!");
            sdcount++;
            display.fillRect(sdpos[0], 10, 246, 40, DARKGREY);
            display.setFont(&fonts::lgfxJapanGothic_40);
            display.setTextSize(1); 
            display.setTextColor(CYAN, DARKGREY);
            display.setCursor(sdpos[0], sdpos[1]);
            display.printf("SD OK %d", sdcount);
        }
        else{
            //Serial.println("ERROR : FILE SAVE ERROR (SD)!");
            sdcount++;
            display.fillRect(sdpos[0], 10, 246, 40, DARKGREY);
            display.setFont(&fonts::lgfxJapanGothic_40);
            display.setTextSize(1); 
            display.setTextColor(RED, DARKGREY);
            display.setCursor(sdpos[0], sdpos[1]);
            display.printf("SD ERROR %d", sdcount);
        }
    }
}


// SD LOAD
// 15360B以下（4×24h×4day×40B＝15360B）
int sdload(char *filename, char *ss)
{
    File fr;
    int size = 0;
    
    // SDカード読込
    if(sdok != 0){
        fr = SD.open(filename, FILE_READ);
        if(!fr){
            //Serial.println("Open failed");
        }
        else{
            size = fr.size();
            if(size > 15360){
                //Serial.println("File Size Error : ");
                //Serial.printf("%d", size);
                //Serial.printf(" > 15360B\n");
            }
            else{
                fr.read((uint8_t*)ss, size);
                fr.close();
            }
        }
    }
    
    return size;
}
