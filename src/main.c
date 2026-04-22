#include <windows.h>
#include <stdio.h>

#define ID_BTN_PING 1001
#define ID_BTN_STOP 1002
#define ID_BTN_TRACERT 1003
#define ID_BTN_SCAN 1004
#define ID_BTN_CLEAR 1005
#define ID_TXT_TARGET 2001
#define ID_TXT_LOG 3001

#define WM_PING_RESULT (WM_USER + 1)
#define WM_PING_COMPLETE (WM_USER + 2)
#define WM_TRACERT_HOP (WM_USER + 3)
#define WM_TRACERT_COMPLETE (WM_USER + 4)
#define WM_SCAN_RESULT (WM_USER + 5)
#define WM_SCAN_COMPLETE (WM_USER + 6)

void RunPing(const char* target);
void StopPing(void);
void RunTracert(const char* target);
void StopTracert(void);
void RunNetworkScan(void);
void StopNetworkScan(void);

HWND hMainWnd;
HWND hLogWnd;
HWND hBtnPing;
HWND hBtnStop;

volatile BOOL bPingRunning = FALSE;
HANDLE hPingThread = NULL;

volatile BOOL bTracertRunning = FALSE;
HANDLE hTracertThread = NULL;

volatile BOOL bScanRunning = FALSE;
HANDLE hScanThread = NULL;

void AddToLog(const char* text) {
    int len = GetWindowTextLengthA(hLogWnd);
    SendMessageA(hLogWnd, EM_SETSEL, len, len);
    SendMessageA(hLogWnd, EM_REPLACESEL, FALSE, (LPARAM)text);
}

void ClearLog(void) {
    SetWindowTextA(hLogWnd, "");
}

void EnablePingControls(BOOL enable) {
    EnableWindow(hBtnPing, enable);
    EnableWindow(hBtnStop, !enable);
    EnableWindow(GetDlgItem(hMainWnd, ID_BTN_TRACERT), enable);
    EnableWindow(GetDlgItem(hMainWnd, ID_BTN_SCAN), enable);
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

        CreateWindowEx(0, "BUTTON", "Scan Rede",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            340, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_SCAN, NULL, NULL);

        CreateWindowEx(0, "BUTTON", "Limpar",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            450, 50, 100, 30,
            hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL);

        hLogWnd = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL | ES_READONLY,
            10, 90, 530, 300,
            hwnd, (HMENU)ID_TXT_LOG, NULL, NULL);

        break;
    }

    case WM_PING_RESULT: {
        char* pMsg = (char*)lParam;
        AddToLog(pMsg);
        HeapFree(GetProcessHeap(), 0, pMsg);
        break;
    }

    case WM_PING_COMPLETE:
        EnablePingControls(TRUE);
        AddToLog("[INFO] Ping finalizado\r\n");
        break;

    case WM_TRACERT_HOP: {
        char* pMsg = (char*)lParam;
        AddToLog(pMsg);
        HeapFree(GetProcessHeap(), 0, pMsg);
        break;
    }

    case WM_TRACERT_COMPLETE:
        bTracertRunning = FALSE;
        EnableWindow(hBtnPing, TRUE);
        EnableWindow(hBtnStop, FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_SCAN), TRUE);
        AddToLog("[INFO] Tracert finalizado\r\n");
        break;

    case WM_SCAN_RESULT: {
        char* pMsg = (char*)lParam;
        AddToLog(pMsg);
        HeapFree(GetProcessHeap(), 0, pMsg);
        break;
    }

    case WM_SCAN_COMPLETE:
        bScanRunning = FALSE;
        EnableWindow(hBtnPing, TRUE);
        EnableWindow(hBtnStop, FALSE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), TRUE);
        EnableWindow(GetDlgItem(hwnd, ID_BTN_SCAN), TRUE);

        int len = GetWindowTextLengthA(hLogWnd);
        char* logText = (char*)malloc(len + 1);
        GetWindowTextA(hLogWnd, logText, len + 1);

        int count = 0;
        char* pos = logText;
        while ((pos = strstr(pos, "ATIVO")) != NULL) {
            count++;
            pos += 5;
        }
        free(logText);

        char summary[64];
        sprintf_s(summary, sizeof(summary), "[INFO] Scan finalizado: %d hosts ativos encontrados\r\n", count);
        AddToLog(summary);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_PING: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0 && !bPingRunning && !bTracertRunning && !bScanRunning) {
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
            else if (bScanRunning) {
                AddToLog("[INFO] Parando scan...\r\n");
                StopNetworkScan();
            }
            break;

        case ID_BTN_TRACERT: {
            char target[256];
            HWND hTarget = GetDlgItem(hwnd, ID_TXT_TARGET);
            GetWindowTextA(hTarget, target, sizeof(target));

            if (strlen(target) > 0 && !bTracertRunning && !bPingRunning && !bScanRunning) {
                bTracertRunning = TRUE;
                EnableWindow(hBtnPing, FALSE);
                EnableWindow(hBtnStop, TRUE);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), FALSE);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_SCAN), FALSE);

                char logMsg[512];
                sprintf_s(logMsg, sizeof(logMsg), "[TRACERT] Rastreando rota para: %s (max 30 hops)\r\n", target);
                AddToLog(logMsg);
                AddToLog("Hop  IP              Tempo\r\n");
                AddToLog("--------------------------------\r\n");

                RunTracert(target);
            }
            break;
        }

        case ID_BTN_SCAN: {
            if (!bScanRunning && !bPingRunning && !bTracertRunning) {
                bScanRunning = TRUE;
                EnableWindow(hBtnPing, FALSE);
                EnableWindow(hBtnStop, TRUE);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_TRACERT), FALSE);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_SCAN), FALSE);

                AddToLog("[SCAN] Iniciando varredura na rede local...\r\n");
                AddToLog("IP Status\r\n");
                AddToLog("--------------------\r\n");

                RunNetworkScan();
            }
            break;
        }

        case ID_BTN_CLEAR:
            ClearLog();
            break;
        }
        break;

    case WM_DESTROY:
        if (bPingRunning) StopPing();
        if (bTracertRunning) StopTracert();
        if (bScanRunning) StopNetworkScan();
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
        CW_USEDEFAULT, CW_USEDEFAULT, 680, 480,
        NULL, NULL, hInstance, NULL
    );

    if (!hMainWnd) {
        MessageBoxA(NULL, "Falha ao criar janela!", "Erro", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    MSG msgLoop;
    while (GetMessage(&msgLoop, NULL, 0, 0) > 0) {
        TranslateMessage(&msgLoop);
        DispatchMessage(&msgLoop);
    }

    return (int)msgLoop.wParam;
}