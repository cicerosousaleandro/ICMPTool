#include <windows.h>
#include <stdio.h>

void RunPing(const char* target);
void RunTracert(const char* target);

#define ID_BTN_PING 1001
#define ID_BTN_TRACERT 1002
#define ID_TXT_TARGET 2001
#define ID_TXT_LOG 3001

HWND hMainWnd;
HWND hLogWnd;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowEx(0, "STATIC", "Target IP/Hostname:",
            WS_CHILD | WS_VISIBLE,
            10, 10, 120, 20,
            hwnd, NULL, NULL, NULL);

        CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            140, 10, 200, 25,
            hwnd, (HMENU)ID_TXT_TARGET, NULL, NULL);

        CreateWindowEx(0, "BUTTON", "Ping",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_PING, NULL, NULL);

        CreateWindowEx(0, "BUTTON", "Tracert",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_TRACERT, NULL, NULL);

        hLogWnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_READONLY,
            10, 90, 530, 300,
            hwnd, (HMENU)ID_TXT_LOG, NULL, NULL);

        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_PING: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0) {
                char logMsg[512];
                sprintf_s(logMsg, sizeof(logMsg), "[PING] Iniciando ping para: %s\r\n", target);
                int len = GetWindowTextLengthA(hLogWnd);
                SendMessageA(hLogWnd, EM_SETSEL, len, len);
                SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)logMsg);

                SendMessageA(hLogWnd, EM_SETSEL, len, len);
                SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)"[INFO] Ping será implementado na Aula 3\r\n");
            }
            break;
        }

        case ID_BTN_TRACERT: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0) {
                char logMsg[512];
                sprintf_s(logMsg, sizeof(logMsg), "[TRACERT] Iniciando tracert para: %s\r\n", target);
                int len = GetWindowTextLengthA(hLogWnd);
                SendMessageA(hLogWnd, EM_SETSEL, len, len);
                SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)logMsg);

                SendMessageA(hLogWnd, EM_SETSEL, len, len);
                SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)"[INFO] Tracert será implementado na Aula 4\r\n");
            }
            break;
        }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow) {

    (void)hPrevInstance;
    (void)lpCmdLine;

    WNDCLASSEXA wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "ICMPToolClass";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Falha ao registrar classe da janela!", "Erro", MB_ICONERROR);
        return 1;
    }

    hMainWnd = CreateWindowExA(
        0,
        "ICMPToolClass",
        "ICMPTool - Network Reconnaissance",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 570, 450,
        NULL, NULL, hInstance, NULL
    );

    if (!hMainWnd) {
        MessageBoxA(NULL, "Falha ao criar janela!", "Erro", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}