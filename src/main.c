#include <windows.h>
#include <stdio.h>


#define ID_BTN_PING 1001
#define ID_BTN_STOP 1002
#define ID_BTN_TRACERT 1003
#define ID_TXT_TARGET 2001
#define ID_TXT_LOG 3001


#define WM_PING_RESULT (WM_USER + 1)
#define WM_PING_COMPLETE (WM_USER + 2)
#define WM_TRACERT_HOP (WM_USER + 3)
#define WM_TRACERT_COMPLETE (WM_USER + 4)


void RunPing(const char* target);
void StopPing(void);
void RunTracert(const char* target);
void StopTracert(void);


HWND hMainWnd;
HWND hLogWnd;
HWND hBtnPing;
HWND hBtnStop;

volatile BOOL bPingRunning = FALSE;
HANDLE hPingThread = NULL;

volatile BOOL bTracertRunning = FALSE;
HANDLE hTracertThread = NULL;


void AddToLog(const char* text) {
    int len = GetWindowTextLengthA(hLogWnd);
    SendMessageA(hLogWnd, EM_SETSEL, len, len);
    SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)text);
}


void EnablePingControls(BOOL enable) {
    EnableWindow(hBtnPing, enable);
    EnableWindow(hBtnStop, !enable);
    
    EnableWindow(GetDlgItem(hMainWnd, ID_BTN_TRACERT), enable);
    bPingRunning = !enable;
}

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

        hBtnPing = CreateWindowEx(0, "BUTTON", "Ping",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_PING, NULL, NULL);

        hBtnStop = CreateWindowEx(0, "BUTTON", "Parar",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            120, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);
        EnableWindow(hBtnStop, FALSE);

        CreateWindowEx(0, "BUTTON", "Tracert",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            230, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_TRACERT, NULL, NULL);

        hLogWnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_READONLY,
            10, 90, 530, 300,
            hwnd, (HMENU)ID_TXT_LOG, NULL, NULL);

        break;
    }

                  
    case WM_PING_RESULT: {
        char* msg = (char*)lParam;
        AddToLog(msg);
        HeapFree(GetProcessHeap(), 0, msg);
        break;
    }

    case WM_PING_COMPLETE:
        EnablePingControls(TRUE);
        AddToLog("[INFO] Ping finalizado\r\n");
        break;

        
    case WM_TRACERT_HOP: {
        char* msg = (char*)lParam;
        AddToLog(msg);
        HeapFree(GetProcessHeap(), 0, msg);
        break;
    }

    case WM_TRACERT_COMPLETE:
        bTracertRunning = FALSE;
        
        EnableWindow(hBtnPing, TRUE);
        EnableWindow(hBtnStop, FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), TRUE);
        AddToLog("[INFO] Tracert finalizado\r\n");
        break;

        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_PING: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0 && !bPingRunning && !bTracertRunning) {
                EnablePingControls(FALSE);
                char logMsg[512];
                sprintf_s(logMsg, sizeof(logMsg), "[PING] Iniciando ping continuo para: %s\r\n", target);
                AddToLog(logMsg);

                RunPing(target);
            }
            break;
        }

        case ID_BTN_STOP:
            if (bPingRunning) {
                AddToLog("[INFO] Parando ping...\r\n");
                StopPing();
            }
            else if (bTracertRunning) {
                AddToLog("[INFO] Parando tracert...\r\n");
                StopTracert();
            }
            break;

        case ID_BTN_TRACERT: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0 && !bTracertRunning && !bPingRunning) {
                bTracertRunning = TRUE;
                
                EnableWindow(hBtnPing, FALSE);
                EnableWindow(hBtnStop, TRUE);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), FALSE);

                char logMsg[512];
                sprintf_s(logMsg, sizeof(logMsg), "[TRACERT] Rastreando rota para: %s (max 30 hops)\r\n", target);
                AddToLog(logMsg);
                AddToLog("Hop  IP              Tempo\r\n");
                AddToLog("--------------------------------\r\n");

                RunTracert(target);
            }
            break;
        }
        }
        break;

    case WM_DESTROY:
        if (bPingRunning) {
            StopPing();
        }
        if (bTracertRunning) {
            StopTracert();
        }
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
        MessageBoxA(NULL, "Falha ao registrar classe!", "Erro", MB_ICONERROR);
        return 1;
    }

    hMainWnd = CreateWindowExA(
        0,
        "ICMPToolClass",
        "ICMPTool - Network Reconnaissance",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 570, 480,
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