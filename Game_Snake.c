/* 
gcc Game_3D_4.c -o Game4.exe -Wall
Game4.exe
*/
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <locale.h>
#include <conio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h> // สำหรับ toupper()

#define X_SCREEN 230
#define Y_SCREEN 75

#define FPS 80
#define Y_i ((Y_SCREEN + 1) & ~1)

// ตัวอักษร (FG): \033[38;2;R;G;Bm
// พื้นหลัง (BG): \033[48;2;R;G;Bm

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    char symbol[4];
}RGB;

char Text_B[] = "▀";
// const long Y_i = (int)ceil(Y_SCREEN / 2);
RGB buffer_color[Y_i][X_SCREEN];
const int siz_X  = (sizeof(buffer_color[0]) / sizeof(buffer_color[0][0]));
const int siz_Y = (sizeof(buffer_color)    / sizeof(buffer_color[0]));
clock_t Renderer_time = 0;
const double time_FPS = 1000 / FPS;
char key;
long text_pos_X = 0, text_pos_Y = 0;
char buffer_Number[64];
uint8_t color_1 = 255, color_2 = 255, color_3 = 255;
POINT pos_Mouse;
static WNDPROC oldProc = NULL;

// --- ส่วนของตัวแปรเก็บสถานะ (Global State) ---
struct {
    bool left_pressed;
    int x;
    int y;
} g_mouse;

HANDLE hIn, hOut;
DWORD origMode;

// --- ฟังก์ชันจัดการ Console Input/Output ---
// Engine Game 2D ทำ 3D ได้แต่คำนวนเอง

LRESULT CALLBACK ConsoleProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_CONTEXTMENU)
    {
        return 0;   // บล็อกเมนูคลิกขวา
    }

    return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
}

void disable_right_click_menu()
{
    HWND hwnd = GetConsoleWindow();

    oldProc = (WNDPROC)SetWindowLongPtr(
        hwnd,
        GWLP_WNDPROC,
        (LONG_PTR)ConsoleProc
    );
}

void disable_quick_edit() {
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);

    DWORD mode;
    GetConsoleMode(hInput, &mode);

    mode &= ~ENABLE_QUICK_EDIT_MODE;   // ปิด QuickEdit
    mode &= ~ENABLE_MOUSE_INPUT;       // (ถ้าไม่อยากให้ console จัดการเมาส์)
    mode |= ENABLE_EXTENDED_FLAGS;     // ต้องมีก่อน set

    SetConsoleMode(hInput, mode);
}

// --- ฟังก์ชันช่วยเหลือ (เรียกใช้ง่ายๆ) ---

// 1. ตั้งค่าเริ่มต้น (เรียกครั้งเดียวตอนเริ่ม program)
void init_input() {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(hIn, &origMode);

    DWORD mode = origMode;
    mode &= ~ENABLE_QUICK_EDIT_MODE; // ปิดการลากคลุมดำเพื่อคัดลอก (เพราะมันจะหยุด loop)
    mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(hIn, mode);
}

// --- แก้ไขฟังก์ชันตั้งค่าให้เหลือแค่อันเดียวแบบชัวร์ๆ ---
void init_system() {
    hIn = GetStdHandle(STD_INPUT_HANDLE);
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    
    // ตั้งค่า Console Mode: ปิด Quick Edit และ เปิด Mouse Input
    DWORD mode;
    GetConsoleMode(hIn, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;      // ปิดลากคลุม
    mode |= ENABLE_MOUSE_INPUT;           // เปิดรับค่าเมาส์ (ห้ามลบ!)
    mode |= ENABLE_EXTENDED_FLAGS;        // จำเป็นสำหรับการตั้งค่า Extended
    SetConsoleMode(hIn, mode);

    // maximize_console(); // ขยายหน้าจอ
    disable_right_click_menu(); // ปิดเมนูคลิกขวา
}

// 2. คืนค่าเดิม (เรียกตอนจะปิด program)
void cleanup_input() {
    SetConsoleMode(hIn, origMode);
}

// 3. อัปเดตสถานะเมาส์ (เรียกไว้บนสุดของ while loop)
void update_mouse() {
    DWORD events = 0;
    GetNumberOfConsoleInputEvents(hIn, &events);

    while (events > 0) {
        INPUT_RECORD ir;
        DWORD read;
        if (ReadConsoleInput(hIn, &ir, 1, &read) && read == 1) {
            if (ir.EventType == MOUSE_EVENT) {
                MOUSE_EVENT_RECORD mer = ir.Event.MouseEvent;
                
                // เก็บตำแหน่ง X, Y
                g_mouse.x = mer.dwMousePosition.X;
                g_mouse.y = mer.dwMousePosition.Y;

                // เช็คว่ากดปุ่มซ้ายอยู่ไหม
                if (mer.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) {
                    g_mouse.left_pressed = true;
                } else {
                    g_mouse.left_pressed = false;
                }
            } else if (ir.EventType == KEY_EVENT) {
                // ถ้ากด ESC ให้จบโปรแกรม (แถมให้)
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) exit(0);
            }
        }
        GetNumberOfConsoleInputEvents(hIn, &events);
    }
}

long pos_Mouse_X() {
    // GetCursorPos(&pos_Mouse);
    update_mouse(); // อัปเดตสถานะเมาส์ก่อนอ่านค่า
    if(g_mouse.x < X_SCREEN) {
        return g_mouse.x;
    } else {
        return X_SCREEN - 1;
    }
}

long pos_Mouse_Y() {
    // GetCursorPos(&pos_Mouse);
    update_mouse(); // อัปเดตสถานะเมาส์ก่อนอ่านค่า
    // return g_mouse.y*2;
    if(g_mouse.y * 2 < Y_SCREEN) {
        return g_mouse.y * 2;
    } else {
        return Y_SCREEN - 1;
    }
}

int is_mouse_pressed() {
    
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {            // ตรวจคลิกซ้าย
        return 1;
    } else if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {     // ตรวจคลิกขวา
        return 3;
    } else if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {     // ตรวจคลิกกลาง
        return 2;
    } else {
        return 0; // ไม่มีการคลิก
    }
}

void maximize_console()
{
    HWND hwnd = GetConsoleWindow();
    ShowWindow(hwnd, SW_MAXIMIZE);
}

void get_mouse_in_window(int *x, int *y)
{
    POINT p;

    GetCursorPos(&p);                 // ตำแหน่งเมาส์ทั้งจอ
    HWND hwnd = GetConsoleWindow();   // หน้าต่างเกม (console)
    ScreenToClient(hwnd, &p);         // แปลงเป็นในหน้าต่าง

    *x = p.x;
    *y = p.y;
}

void set_console_size(short width, short height)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

    // ตั้ง buffer
    COORD size = { width, height };
    SetConsoleScreenBufferSize(h, size);

    // ตั้ง window
    SMALL_RECT rect = { 0, 0, width - 1, height - 1 };
    SetConsoleWindowInfo(h, TRUE, &rect);
}

void delay(long milliseconds) {
    Sleep(milliseconds);
}

// A-Z ตัวใหญ่เท่านั้น (พิมพ์เล็กไม่มี)
bool is_key_pressed(char key) {
    static SHORT last_state[256] = {0};

    int vk = toupper((unsigned char)key);
    SHORT current = GetAsyncKeyState(vk);

    bool pressed = (current & 0x8000) && !(last_state[vk] & 0x8000);

    last_state[vk] = current;
    return pressed;
}

int random_between(int min, int max) {
    return min + rand() % (max - min + 1);
}

void SetColor(long x, long y, uint8_t R, uint8_t G, uint8_t B) {
    if (x >= 0 && x < X_SCREEN && y >= 0 && y < Y_i) {
        buffer_color[y][x].r = R;
        buffer_color[y][x].g = G;
        buffer_color[y][x].b = B;
    }
}

void add(long x, long y, const char *c) {
    if (x >= 0 && x < X_SCREEN && y >= 0 && y < Y_i) {
        memcpy(buffer_color[y][x].symbol, c, 4);
    }
}

void Clear() {
    for(long y = 0; y < Y_i; y++) {
        for(long x = 0; x < X_SCREEN; x++) {
            SetColor(x, y, 12, 12, 12);
            // SetColor(x, y, 0, 0, 0);
            add(x, y, Text_B);
        }
    }
}

void Renderer_Buffer() {
    if(clock() - Renderer_time > time_FPS) {
        Renderer_time = clock();
        long len_Buffer = ((Y_i * X_SCREEN) * 42) + Y_i;
        char outBuffer[len_Buffer];
        long pos = 0;

        pos += sprintf(&outBuffer[pos], "\x1b[H");
        for(long y = 0; y < siz_Y; y+=2) {
            for(long x = 0; x < siz_X; x++) {
                pos += sprintf(&outBuffer[pos],
                    "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm%s\033[0m",
                    buffer_color[y][x].r,
                    buffer_color[y][x].g,
                    buffer_color[y][x].b,
                    buffer_color[y+1][x].r,
                    buffer_color[y+1][x].g,
                    buffer_color[y+1][x].b,
                    buffer_color[y][x].symbol); // screen_Buffer[][] ต้องเป็น wchar_t
            }
            outBuffer[pos++] = '\n';
        }
        // pos += sprintf(&outBuffer[pos], "\033[0m\0");
        pos += sprintf(&outBuffer[pos], "\033[0m");
        printf("%s", outBuffer);
    }

    // if(is_key_pressed('Q')) {
    //     cleanup_input();
    //     exit(0);   /* จบโปรแกรมทันที (สำเร็จ) */
    // }
}

void Set_pos(long x, long y) {
    text_pos_X = x;
    text_pos_Y = y;
}

void SetColor_Text(long X_point, long Y_point) {
    SetColor(X_point, Y_point, color_1, color_2, color_3);
}

void SetColor_Text_Color(uint8_t color_R, uint8_t color_G, uint8_t color_B) {
    color_1 = color_R;
    color_2 = color_G;
    color_3 = color_B;
}

void numToStr(long X_point) {
    sprintf(buffer_Number, "%ld", X_point);
}

// pixe 5x3
void print_text(const char text) {
    if(text == ' ') {
    } else if(text == 'A') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'B') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'C') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'D') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
    
    } else if(text == 'E') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'F') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);

    } else if(text == 'G') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'H') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'I') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'J') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == 'K') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'L') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'M') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'N') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'O') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'P') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);

    } else if(text == 'Q') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'R') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'S') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'T') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == 'U') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'V') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == 'W') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'X') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'Y') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == 'Z') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == '0') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '1') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == '2') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == '3') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '4') {
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '5') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '6') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '7') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == '8') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == '9') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == 'a') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'b') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'c') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'd') {
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == 'e') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'f') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        
    } else if(text == 'g') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'h') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);
        
    } else if(text == 'i') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        
    } else if(text == 'j') {
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'k') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'l') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'm') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);
        
    } else if(text == 'n') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'o') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'p') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        
    } else if(text == 'q') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'r') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        
    } else if(text == 's') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        
    } else if(text == 't') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        
    } else if(text == 'u') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'v') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        
    } else if(text == 'w') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == 'x') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);
        
    } else if(text == 'y') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        
    } else if(text == 'z') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        
    } else if(text == '\'') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        
    } else if(text == '\"') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);

    } else if(text == '.') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == ',') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
    } else if(text == '!') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);

    } else if(text == '=') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);

    } else if(text == '+') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);

    } else if(text == '-') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);

    } else if(text == '/') {
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);

    } else if(text == '\\') {
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    } else if(text == '?') {
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);

    } else if(text == ':') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);

    } else if(text == ';') {
        SetColor_Text(text_pos_X + 2, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);

    } else {
        // สำหรับตัวอักษรอื่นๆ ที่ไม่ได้กำหนดไว้
        SetColor_Text(text_pos_X + 0, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 0);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 1);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 2);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 3);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 4);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 5);
        SetColor_Text(text_pos_X + 0, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 1, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 2, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 3, text_pos_Y + 6);
        SetColor_Text(text_pos_X + 4, text_pos_Y + 6);

    }
}

void print_number(const char *text) {
    long X_pointer_N = text_pos_X;
    while(*text != '\0') {
        if(*text == '\n') {
            text++;
            Set_pos(X_pointer_N, text_pos_Y + 8); // Move to the next line (assuming each character is 7 units tall)
            // text_pos_Y += 7; // Move to the next line (assuming each character is 7 units tall)
        } else if(*text == '\t') {
            text++;
            Set_pos(text_pos_X + 30, text_pos_Y); // Move to the next tab position (assuming each tab is 4 characters wide)
            // text_pos_X += 24; // Move to the next tab position (assuming each tab is 4 characters wide)

        } else {
            print_text(*text);
            text++;
            Set_pos(text_pos_X + 6, text_pos_Y); // Move to the next position (assuming each character is 6 units wide)
            // text_pos_X += 6; // Move to the next position (assuming each character is 6 units wide)
        }
    }
}

void print_char(char c) {
    // putchar(c);   // เปลี่ยนเป็น renderer นายได้
    print_text(c);
}

void print_str(const char *s) {
    while (*s) print_char(*s++);
}

void print_int(int v) {
    // printf("%d", v);   // ชั่วคราวก่อน นายค่อยเขียนเอง
    sprintf(buffer_Number, "%d", v);
    print_number(buffer_Number);
}

void print_long(long v) {
    sprintf(buffer_Number, "%ld", v);
    print_number(buffer_Number);
}

void print_float_2(float v) {
    sprintf(buffer_Number, "%.2f", v);
    print_number(buffer_Number);
}

void print_float_6(float v) {
    sprintf(buffer_Number, "%.6f", v);
    print_number(buffer_Number);
}

void print_float_15(float v) {
    sprintf(buffer_Number, "%.15f", v);
    print_number(buffer_Number);
}

void printf_text(const char *fmt, ...) {
    long X_pointer_N = text_pos_X;
    va_list args;
    va_start(args, fmt); // ← ต้องมีเพื่อเริ่มต้นการใช้งาน va_list
    while (*fmt) {

        if (*fmt == '%') {
            fmt++;
            if (!*fmt) break;   // กัน format หลุด

            switch (*fmt) {
                case 'd': print_int((int)va_arg(args, int)); break;
                case 'c': print_char((char)va_arg(args, int)); break;
                case 's': print_str(va_arg(args, char*)); break;
                case 'f': print_float_2(va_arg(args, double)); break;
                case 'F': print_float_6(va_arg(args, double)); break;
                case 'D': print_float_15(va_arg(args, double)); break;
                case 'l': print_long(va_arg(args, long)); break;
                case '%': print_char('%'); break;
                default:
                    print_char('%');
                    print_char(*fmt);
            }

            fmt++;
            continue;
        }

        if (*fmt == '\n') {
            Set_pos(X_pointer_N, text_pos_Y + 8);
            fmt++;
            continue;
        }

        if (*fmt == '\t') {
            Set_pos(text_pos_X + 30, text_pos_Y);
            fmt++;
            continue;
        }

        print_char(*fmt);
        Set_pos(text_pos_X + 6, text_pos_Y);
        fmt++;
    }
    va_end(args);        // ← ต้องมีเพื่อสิ้นสุดการใช้งาน va_list
}

void draw_Square(int xx_1, int yy_1, int xx_2, int yy_2) {
    for(int i = xx_1; i < xx_2; i++) {
        SetColor(i, yy_1, 255, 255, 255); 
        SetColor(i, yy_2, 255, 255, 255);
    }
    for(int i = yy_1; i < yy_2; i++) {
        SetColor(xx_1, i, 255, 255, 255); 
        SetColor(xx_2 - 1, i, 255, 255, 255);
    }
}

void draw_Square_color(int xx_1, int yy_1, int xx_2, int yy_2, int r, int g, int b) {
    for(int i = xx_1; i < xx_2; i++) {
        SetColor(i, yy_1, r, g, b);
        SetColor(i, yy_2, r, g, b);
    }
    for(int i = yy_1; i < yy_2; i++) {
        SetColor(xx_1, i, r, g, b); 
        SetColor(xx_2 - 1, i, r, g, b);
    }
}

void draw_Square_color_Full(int xx_1, int yy_1, int xx_2, int yy_2, int r1, int g1, int b1, int r2, int g2, int b2) {

    for(int j = 0; j < (yy_2 - yy_1); j++) {
        for(int i = xx_1; i < xx_2; i++) {
            SetColor(i, yy_1 + j, r2, g2, b2);
        }
    }
    draw_Square_color(xx_1, yy_1, xx_2, yy_2, r1, g1, b1);
}

int is_Mouse_Over(int xx_1, int yy_1, int xx_2, int yy_2) {
    return (pos_Mouse_X() > xx_1 && pos_Mouse_X() < xx_2 && pos_Mouse_Y() > yy_1 && pos_Mouse_Y() < yy_2);
}

void init() {
    // maximize_console();
    // init_input();
    set_console_size(X_SCREEN, (int)((Y_SCREEN/2)+2));
    // disable_quick_edit();
    // disable_right_click_menu();
    srand(time(NULL));
    system("chcp 65001");
    system("cls");
    printf("\x1b[?25l"); // hide cursor
    init_system();
    Clear();
}

int mouse_Click() {
    return is_mouse_pressed() == 1;
}

// ^^ Engine Game
// Code Game

int color_Game_R = 255;
int color_Game_G = 255;
int color_Game_B = 255;
int RGBMode = 0;

/*
ตำแหน่งเริ่มต้นและจบของงู :
    min_X = 53
    min_Y = 4
    max_X = 224
    max_Y = 70
*/
int Px = 53;
int Py = 4;
int Pt = 4;
/*  ^^ Pt คือทิศทางของงู
    0 = ขึ้น
    1 = ลง
    2 = ซ้าย
    3 = ขวา
*/

// buffer สำหรับเก็บตำแหน่งของงู
int Pxy_Buffer[(X_SCREEN - 58) * (Y_SCREEN - 9)][2];

// ความยาวเริ่มต้นของงู
int Score = 0;
int Score_pos[2];

void Set_pos_Score() {
    Score_pos[0] = random_between(53, (X_SCREEN - 6));  // X Score
    Score_pos[1] = random_between(4, (Y_SCREEN - 6));   // Y Score

    for(int i = 0; i < Score; i++) {
        if(Score_pos[0] == Pxy_Buffer[i][0] && Score_pos[1] == Pxy_Buffer[i][1]) {
            Score_pos[0] = random_between(53, (X_SCREEN - 6));  // X Score
            Score_pos[1] = random_between(4, (Y_SCREEN - 6));   // Y Score
            i = -1; // รีเซ็ตการเช็คใหม่ทั้งหมด
        }

    }
}

// ฟังก์ชันคำนวณตำแหน่งของงูและอัพเดต Score
int Calculate_position() {
    // เช็ค งูเก็ยบ Score หรือยัง
    if(Pxy_Buffer[0][0] >= (Score_pos[0] - 1) && Pxy_Buffer[0][0] <= Score_pos[0] + 1) {            // X
        if(Pxy_Buffer[0][1] >= (Score_pos[1] - 1) && Pxy_Buffer[0][1] <= (Score_pos[1] + 1)) {      // Y
            Score++;
            Set_pos_Score();
        }
    }

    if(Score > 0 && Pt != 4) {
        for(int i = Score; i > 0; i--) {
            Pxy_Buffer[i][0] = Pxy_Buffer[i - 1][0];
            Pxy_Buffer[i][1] = Pxy_Buffer[i - 1][1];
        }
    }

    switch(Pt) {
        case 0: Pxy_Buffer[0][1]-=2; break;
        case 1: Pxy_Buffer[0][1]+=2; break;
        case 2: Pxy_Buffer[0][0]-=2; break;
        case 3: Pxy_Buffer[0][0]+=2; break;
        case 4: break; // ถ้า Pt = -1 งูจะไม่ขยับ
    }

    // X
    if(Pxy_Buffer[0][0] <= 52) {
        Pxy_Buffer[0][0] = (X_SCREEN - 6);
    } else if(Pxy_Buffer[0][0] >= (X_SCREEN - 5)) {
        Pxy_Buffer[0][0] = 53;
    }

    // Y
    if(Pxy_Buffer[0][1] <= 3) {
        Pxy_Buffer[0][1] = (Y_SCREEN - 6);
    } else if(Pxy_Buffer[0][1] >= (Y_SCREEN - 5)) {
        Pxy_Buffer[0][1] = 4;
    }

    if(Score > 0 && Pt != 4) {
        for(int i = 1; i <= Score; i++) {
            if(Pxy_Buffer[0][0] == Pxy_Buffer[i][0] && Pxy_Buffer[0][1] == Pxy_Buffer[i][1]) {
                Score = 0;
                break;
            }
        }
    }

    return Score;
}

// หน้าเล่นเกม
void play_Game() {
    Clear();
    Renderer_Buffer();
    while(!is_key_pressed('Q')) {
        // ทำกรอบเกม
        Clear();
        draw_Square(0, 0, X_SCREEN, Y_SCREEN);
        
        if(RGBMode == 1) {
            color_Game_R = random_between(0, 255);
            color_Game_G = random_between(0, 255);
            color_Game_B = random_between(0, 255);
        }
        draw_Square_color(2, 2, 50, Y_SCREEN - 2, color_Game_R, color_Game_G, color_Game_B);
        draw_Square_color(51, 2, X_SCREEN - 2, Y_SCREEN - 2, color_Game_R, color_Game_G, color_Game_B);

        if(is_key_pressed('W') && Pt != 1) {
            Pt = 0;
        } else if(is_key_pressed('S') && Pt != 0) {
            Pt = 1;
        } else if(is_key_pressed('A') && Pt != 3) {
            Pt = 2;
        } else if(is_key_pressed('D') && Pt != 2) {
            Pt = 3;
        }

        Calculate_position();
        draw_Square_color(Score_pos[0], Score_pos[1], Score_pos[0]+2, Score_pos[1]+1, 255, 15, 15);
        
        draw_Square_color(Pxy_Buffer[0][0], Pxy_Buffer[0][1], Pxy_Buffer[0][0]+2, Pxy_Buffer[0][1]+1, 15, 200, 15);
        for(int i = 1; i <= Score; i++) {
            draw_Square_color(Pxy_Buffer[i][0], Pxy_Buffer[i][1], Pxy_Buffer[i][0]+2, Pxy_Buffer[i][1]+1, 15, 255, 15);
        }

        Set_pos(4, 4);
        printf_text("Score :\n%d", Score);

        Renderer_Buffer();
        int HS = (int)((500-(Score/2))/10);
        if(HS < 10) { HS = 10; }
        delay(HS);
    }
    while(is_key_pressed('Q')); // รอให้ปล่อยปุ่ม Q ก่อนจะออกจากเกม
    Pt = 4; // หยุดงูไม่ให้ขยับหลังจากออกจากเกม
    Clear();
    Renderer_Buffer();
}

void Manu_Setting() {
    Clear();
    Renderer_Buffer();
    while(!is_key_pressed('Q')) {
        Clear();
        draw_Square(0, 0, X_SCREEN, Y_SCREEN);
        
        Set_pos(2, 2);
        printf_text("RGB Mode : ");

        if(is_Mouse_Over(60, 2, 69, 9) && mouse_Click()) {
            RGBMode = !RGBMode;
            color_Game_R = 255;
            color_Game_G = 255;
            color_Game_B = 255;
            while(mouse_Click());
        }

        if(RGBMode == 1) {
            draw_Square_color_Full(60, 2, 69, 9, 255, 255, 255, 0, 255, 0);
        } else if(RGBMode == 0) {
            draw_Square_color(60, 2, 69, 9, 255, 255, 255);
        }

        Renderer_Buffer();
    }
    while(is_key_pressed('Q'));
    Clear();
    Renderer_Buffer();
}

// ทำเกมงู
int main() {
    init();

    Set_pos_Score();
    Pxy_Buffer[0][0] = random_between(53, (X_SCREEN - 6));  // X Score
    Pxy_Buffer[0][1] = random_between(4, (Y_SCREEN - 6));   // Y Score

    int xx = ((int)(X_SCREEN/2)) - 20;
    int yy = ((int)(Y_SCREEN/2)) + 10;

    while(true) {
        Clear();
        // ชื่อเกม
        Set_pos(((int)(X_SCREEN/2)) - 35, ((int)(Y_SCREEN/2)) - 12);
        SetColor_Text_Color(255, 50, 50);
        printf_text("S");
        SetColor_Text_Color(255, 255, 255);
        printf_text("nake game\n");

        // กรอบเกม
        draw_Square(0, 0, X_SCREEN, Y_SCREEN);
        // Renderer_Buffer();

        Set_pos(xx, yy);
        printf_text("Start");
        if(!is_Mouse_Over(xx-2, yy-2, xx+30, yy+9)) {
            draw_Square(xx-2, yy-2, xx+30, yy+9);
        } else {
            if(mouse_Click()) {
                play_Game();
            }
        }

        Set_pos(4, Y_SCREEN - 10);
        printf_text("Settings");
        if(!is_Mouse_Over(2, Y_SCREEN-12, 52, Y_SCREEN-2)) {
            draw_Square(2, Y_SCREEN-12, 52, Y_SCREEN-2);
        } else {
            if(mouse_Click()) {
                Manu_Setting();
            }
        }

        // Set_pos(2, 2);
        // printf_text("is key : %d", is_key_pressed('q'));
        Renderer_Buffer();
    }

    // Reset color มีไว้เผื่อมีการเปลี่ยนสีแล้วลืมเปลี่ยนกลับ
    printf("\033[0m");
    return 0;
}
