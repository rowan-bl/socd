#include <windows.h>
#include <stdio.h>
#include <shlwapi.h>
#include "resource.h"
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib,"gdi32.lib")

#define DIRECTION_LEFT 0
#define DIRECTION_RIGHT 1
#define DIRECTION_UP 2
#define DIRECTION_DOWN 3
#define DIRECTION_EXTRA_1 4  // Q
#define DIRECTION_EXTRA_2 5  // E

#define IS_DOWN 1
#define IS_UP 0
#define whitelist_max_length 200
#define WASD_ID 100
#define RSIX_ID 200
#define CUSTOM_ID 300

#define KEY_W 0x57
#define KEY_A 0x41
#define KEY_S 0x53
#define KEY_D 0x44
#define KEY_E 0x45
#define KEY_Q 0x51
#define KEY_DISABLED 0x00

HWND main_window;

const char* CONFIG_NAME = "socd.conf";
const LPCWSTR CLASS_NAME = L"SOCD_CLASS";
char config_line[100];
char focused_program[MAX_PATH];
char programs_whitelist[whitelist_max_length][MAX_PATH] = { 0 };

HHOOK kbhook;
int hook_is_installed = 0;

int real[6]; // whether the key is pressed for real on keyboard
int virtual[6]; // whether the key is pressed on a software level
int DEFUALT_DISABLE_BIND = KEY_DISABLED;
int WASD[6] = { KEY_A, KEY_D, KEY_W, KEY_S, KEY_DISABLED, KEY_DISABLED };
int RSIX[6] = { KEY_A, KEY_D, KEY_W, KEY_S, KEY_Q, KEY_E }; // Q and E included here
int CUSTOM_BINDS[6];
int DISABLE_BIND;
int disableKeyPressed;

HFONT hFontBold, hFontSmall;

int show_error_and_quit(char* text) {
    int error = GetLastError();
    char error_buffer[256];
    sprintf(error_buffer, text, error);
    MessageBox(
        NULL,
        error_buffer,
        "RIP",
        MB_OK | MB_ICONERROR);
    ExitProcess(1);
    return 0; // Shouldn't reach here, but just to satisfy the compiler
}

void write_settings(int* bindings, int disableBind) {
    FILE* config_file = fopen(CONFIG_NAME, "w");
    if (config_file == NULL) {
        perror("Couldn't open the config file");
        return;
    }
    for (int i = 0; i < 6; i++) {
        fprintf(config_file, "%X\n", bindings[i]);
    }
    fprintf(config_file, "%X\n", disableBind);
    for (int i = 0; i < whitelist_max_length; i++) {
        if (programs_whitelist[i][0] == '\0') {
            break;
        }
        fprintf(config_file, "%s\n", programs_whitelist[i]);
    }
    fclose(config_file);
}

void set_bindings(int* bindings, int disableBind) {
    for (int i = 0; i < 6; i++) {
        CUSTOM_BINDS[i] = bindings[i];
    }
    DISABLE_BIND = disableBind;
}

void read_settings() {
    FILE* config_file = fopen(CONFIG_NAME, "r+");
    if (config_file == NULL) {
        set_bindings(WASD, DEFUALT_DISABLE_BIND);
        write_settings(WASD, DEFUALT_DISABLE_BIND);
        return;
    }

    for (int i = 0; i < 6; i++) {
        char* result = fgets(config_line, 100, config_file);
        int button = (int)strtol(result, NULL, 16);
        CUSTOM_BINDS[i] = button;
    }

    char* result = fgets(config_line, 100, config_file);
    DISABLE_BIND = (int)strtol(result, NULL, 16);

    int i = 0;
    while (fgets(programs_whitelist[i], MAX_PATH, config_file) != NULL) {
        programs_whitelist[i][strcspn(programs_whitelist[i], "\r\n")] = 0;
        i++;
    }
    fclose(config_file);
}

int find_opposing_key(int key) {
    if (key == CUSTOM_BINDS[DIRECTION_LEFT]) {
        return CUSTOM_BINDS[DIRECTION_RIGHT];
    }
    if (key == CUSTOM_BINDS[DIRECTION_RIGHT]) {
        return CUSTOM_BINDS[DIRECTION_LEFT];
    }
    if (key == CUSTOM_BINDS[DIRECTION_UP]) {
        return CUSTOM_BINDS[DIRECTION_DOWN];
    }
    if (key == CUSTOM_BINDS[DIRECTION_DOWN]) {
        return CUSTOM_BINDS[DIRECTION_UP];
    }
    if (key == CUSTOM_BINDS[DIRECTION_EXTRA_1]) {  // Q
        return CUSTOM_BINDS[DIRECTION_EXTRA_2];    // E
    }
    if (key == CUSTOM_BINDS[DIRECTION_EXTRA_2]) {  // E
        return CUSTOM_BINDS[DIRECTION_EXTRA_1];    // Q
    }
    return -1;
}

int find_index_by_key(int key) {
    for (int i = 0; i < 6; i++) {
        if (key == CUSTOM_BINDS[i]) {
            return i;
        }
    }
    return -1;
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* kbInput = (KBDLLHOOKSTRUCT*)lParam;

    if (nCode != HC_ACTION || (kbInput->flags & LLKHF_INJECTED)) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    int key = kbInput->vkCode;
    if (key == DISABLE_BIND) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            disableKeyPressed = IS_DOWN;
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            disableKeyPressed = IS_UP;
        }
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    int opposing = find_opposing_key(key);
    int index = find_index_by_key(key);
    int opposing_index = (opposing >= 0) ? find_index_by_key(opposing) : -1;

    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    input.ki.dwFlags = KEYEVENTF_SCANCODE;

    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        real[index] = IS_DOWN;
        virtual[index] = IS_DOWN;

        if (opposing_index >= 0 && real[opposing_index] == IS_DOWN && virtual[opposing_index] == IS_DOWN && disableKeyPressed == IS_UP) {
            input.ki.wScan = MapVirtualKeyW(opposing, MAPVK_VK_TO_VSC);
            input.ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(1, &input, sizeof(INPUT));
            virtual[opposing_index] = IS_UP;
        }
    }
    else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        real[index] = IS_UP;
        virtual[index] = IS_UP;

        if (opposing_index >= 0 && real[opposing_index] == IS_DOWN && disableKeyPressed == IS_UP) {
            input.ki.wScan = MapVirtualKeyW(opposing, MAPVK_VK_TO_VSC);
            input.ki.dwFlags = KEYEVENTF_SCANCODE;
            SendInput(1, &input, sizeof(INPUT));
            virtual[opposing_index] = IS_DOWN;
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void set_kb_hook(HINSTANCE instance) {
    if (!hook_is_installed) {
        kbhook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, instance, 0);
        if (kbhook != NULL) {
            hook_is_installed = 1;
        }
        else {
            show_error_and_quit("Hook installation failed, error code is %d");
        }
    }
}

void unset_kb_hook() {
    if (hook_is_installed) {
        UnhookWindowsHookEx(kbhook);
        for (int i = 0; i < 6; i++) {
            real[i] = IS_UP;
            virtual[i] = IS_UP;
        }
        hook_is_installed = 0;
    }
}

void get_focused_program() {
    HWND inspected_window = GetForegroundWindow();
    DWORD process_id = 0;
    GetWindowThreadProcessId(inspected_window, &process_id);
    if (process_id == 0) {
        return;
    }
    HANDLE hproc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, 0, process_id);
    if (hproc == NULL) {
        if (GetLastError() == 5) {
            return;
        }
        show_error_and_quit("Couldn't open active process, error code is: %d");
    }
    DWORD filename_size = MAX_PATH;
    QueryFullProcessImageName(hproc, 0, focused_program, &filename_size);
    CloseHandle(hproc);
    PathStripPath(focused_program);
    printf("Window activated: %s\n", focused_program);
}

void detect_focused_program(
    HWINEVENTHOOK hWinEventHook,
    DWORD event,
    HWND window,
    LONG idObject,
    LONG idChild,
    DWORD idEventThread,
    DWORD dwmsEventTime
) {
    get_focused_program();

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    for (int i = 0; i < whitelist_max_length; i++) {
        if (strcmp(focused_program, programs_whitelist[i]) == 0) {
            set_kb_hook(hInstance);
            return;
        }
        else if (programs_whitelist[i][0] == '\0') {
            break;
        }
    }
    unset_kb_hook();
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_COMMAND:
        SetFocus(main_window);
        if (wParam == WASD_ID) {
            set_bindings(WASD, DEFUALT_DISABLE_BIND);
            write_settings(WASD, DEFUALT_DISABLE_BIND);
            return 0;
        }
        else if (wParam == RSIX_ID) {
            set_bindings(RSIX, DEFUALT_DISABLE_BIND);
            write_settings(RSIX, DEFUALT_DISABLE_BIND);
            return 0;
        }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(0, 0, 0));
        SetBkMode(hdcStatic, TRANSPARENT);
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(WHITE_BRUSH));
        EndPaint(hwnd, &ps);
        break;
    }
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int main() {
    FreeConsole();

    disableKeyPressed = IS_UP;
    for (int i = 0; i < 6; i++) {
        real[i] = IS_UP;
        virtual[i] = IS_UP;
    }

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

    hFontBold = CreateFont(
        20,
        0,
        0,
        0,
        FW_BOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");

    hFontSmall = CreateFont(
        16,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        L"Segoe UI");

    read_settings();
    if (programs_whitelist[0][0] == '\0') {
        set_kb_hook(hInstance);
    }
    else {
        SetWinEventHook(
            EVENT_OBJECT_FOCUS,
            EVENT_OBJECT_FOCUS,
            hInstance,
            (WINEVENTPROC)detect_focused_program,
            0,
            0,
            WINEVENT_OUTOFCONTEXT
        );
    }

    WNDCLASSEXW wc;
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

    if (RegisterClassExW(&wc) == 0) {
        show_error_and_quit("Failed to register window class, error code is %d");
    };

    main_window = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Last Input Priority",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        450,
        242,
        NULL,
        NULL,
        hInstance,
        NULL);
    if (main_window == NULL) {
        show_error_and_quit("Failed to create a window, error code is %d");
    }

    HWND wasd_hwnd = CreateWindowExW(
        0,
        L"BUTTON",
        L"WASD",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        10,
        108,
        150,
        30,
        main_window,
        (HMENU)WASD_ID,
        hInstance,
        NULL);
    if (wasd_hwnd == NULL) {
        show_error_and_quit("Failed to create WASD radiobutton, error code is %d");
    }
    SendMessageW(wasd_hwnd, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

    HWND arrows_hwnd = CreateWindowExW(
        0,
        L"BUTTON",
        L"WASD + QE (LEANING)",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        10,
        138,
        180,
        30,
        main_window,
        (HMENU)RSIX_ID,
        hInstance,
        NULL);
    if (arrows_hwnd == NULL) {
        show_error_and_quit("Failed to create WASD + QE radiobutton, error code is %d");
    }
    SendMessageW(arrows_hwnd, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

    HWND custom_hwnd = CreateWindowExW(
        0,
        L"BUTTON",
        L"Custom",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
        10,
        168,
        150,
        30,
        main_window,
        (HMENU)CUSTOM_ID,
        hInstance,
        NULL);
    if (custom_hwnd == NULL) {
        show_error_and_quit("Failed to create Custom radiobutton, error code is %d");
    }
    SendMessageW(custom_hwnd, WM_SETFONT, (WPARAM)hFontSmall, TRUE);

    int check_id;
    if (memcmp(CUSTOM_BINDS, WASD, sizeof(WASD)) == 0) {
        check_id = WASD_ID;
    }
    else if (memcmp(CUSTOM_BINDS, RSIX, sizeof(RSIX)) == 0) {
        check_id = RSIX_ID;
    }
    else {
        check_id = CUSTOM_ID;
    }
    if (CheckRadioButton(main_window, WASD_ID, CUSTOM_ID, check_id) == 0) {
        show_error_and_quit("Failed to select default keybindings, error code is %d");
    }

    HWND text_hwnd = CreateWindowExW(
        0,
        L"STATIC",
        L"For custom configs:\n"
        L"It uses virtual key codes, stripping the 0x hex prefix\n"
        L"The order is a1, a2, b1, b2, c1, c2, disable key\n",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        10,
        10,
        580,
        110,
        main_window,
        (HMENU)100,
        hInstance,
        NULL);
    if (text_hwnd == NULL) {
        show_error_and_quit("Failed to create Text, error code is %d");
    }
    SendMessageW(text_hwnd, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hFontBold);
    DeleteObject(hFontSmall);

    return 0;
}
