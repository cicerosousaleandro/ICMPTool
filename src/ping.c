#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>

#define WM_PING_RESULT (WM_USER + 1)
#define WM_PING_COMPLETE (WM_USER + 2)

extern HWND hMainWnd;

volatile BOOL bStopPing = FALSE;
extern HANDLE hPingThread;

void SendPingResult(const char* msg) {
    size_t len = strlen(msg) + 1;
    char* msgCopy = (char*)HeapAlloc(GetProcessHeap(), 0, len);
    if (msgCopy) {
        strcpy_s(msgCopy, len, msg);
        PostMessageA(hMainWnd, WM_PING_RESULT, 0, (LPARAM)msgCopy);
    }
}

DWORD WINAPI PingWorkerThread(LPVOID lpParam) {
    char* target = (char*)lpParam;
    HANDLE hIcmpFile;
    char SendData[32] = "ICMPTool";
    LPVOID ReplyBuffer;
    DWORD ReplySize;
    int pingCount = 0;

    hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        SendPingResult("[ERRO] Falha ao criar handle ICMP. Execute como Administrador.\r\n");
        PostMessageA(hMainWnd, WM_PING_COMPLETE, 0, 0);
        HeapFree(GetProcessHeap(), 0, target);
        return 1;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ReplySize);

    // Loop infinito até bStopPing ser TRUE
    while (!bStopPing) {
        pingCount++;

        if (ReplyBuffer) {
            DWORD status = IcmpSendEcho(
                hIcmpFile,
                inet_addr(target),
                SendData,
                sizeof(SendData),
                NULL,
                ReplyBuffer,
                ReplySize,
                2000
            );

            char resultMsg[512];

            if (status != 0) {
                PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;

                sprintf_s(resultMsg, sizeof(resultMsg),
                    "#%d Resposta de %s: bytes=%d tempo=%lums TTL=%d\r\n",
                    pingCount, target,
                    pEchoReply->DataSize,
                    pEchoReply->RoundTripTime,
                    pEchoReply->Options.Ttl
                );
            }
            else {
                DWORD error = GetLastError();
                if (error == 11010 || error == ERROR_TIMEOUT) {
                    sprintf_s(resultMsg, sizeof(resultMsg),
                        "#%d Esgotado tempo limite para %s\r\n", pingCount, target);
                }
                else {
                    sprintf_s(resultMsg, sizeof(resultMsg),
                        "#%d Falha para %s (erro %lu)\r\n",
                        pingCount, target, error);
                }
            }

            SendPingResult(resultMsg);
        }

        // Aguarda ~1 segundo entre pings, mas verifica interrupção a cada 100ms
        for (int i = 0; i < 10 && !bStopPing; i++) {
            Sleep(100);
        }
    }

    HeapFree(GetProcessHeap(), 0, ReplyBuffer);
    IcmpCloseHandle(hIcmpFile);
    HeapFree(GetProcessHeap(), 0, target);

    PostMessageA(hMainWnd, WM_PING_COMPLETE, 0, 0);
    return 0;
}

void RunPing(const char* target) {
    bStopPing = FALSE;

    size_t len = strlen(target) + 1;
    char* targetCopy = (char*)HeapAlloc(GetProcessHeap(), 0, len);
    if (targetCopy) {
        strcpy_s(targetCopy, len, target);
        hPingThread = CreateThread(NULL, 0, PingWorkerThread, targetCopy, 0, NULL);
    }
}

void StopPing(void) {
    bStopPing = TRUE;
    if (hPingThread != NULL) {
        WaitForSingleObject(hPingThread, 3000);
        CloseHandle(hPingThread);
        hPingThread = NULL;
    }
    PostMessageA(hMainWnd, WM_PING_COMPLETE, 0, 0);
}