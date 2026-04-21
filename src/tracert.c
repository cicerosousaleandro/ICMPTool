#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>


#define WM_PING_RESULT (WM_USER + 1)
#define WM_TRACERT_HOP (WM_USER + 3)
#define WM_TRACERT_COMPLETE (WM_USER + 4)

extern HWND hMainWnd;

volatile BOOL bStopTracert = FALSE;
extern HANDLE hTracertThread;

void SendTracertHop(int hopNum, const char* ip, DWORD rtt) {
    char msg[256];
    sprintf_s(msg, sizeof(msg), "%2d  %s  %lu ms\r\n", hopNum, ip, rtt);

    size_t len = strlen(msg) + 1;
    char* msgCopy = (char*)HeapAlloc(GetProcessHeap(), 0, len);
    if (msgCopy) {
        strcpy_s(msgCopy, len, msg);
        PostMessageA(hMainWnd, WM_TRACERT_HOP, 0, (LPARAM)msgCopy);
    }
}

void SendTracertComplete() {
    PostMessageA(hMainWnd, WM_TRACERT_COMPLETE, 0, 0);
}

DWORD WINAPI TracertWorkerThread(LPVOID lpParam) {
    char* target = (char*)lpParam;
    HANDLE hIcmpFile;
    char SendData[32] = "ICMPTool Tracert";
    LPVOID ReplyBuffer;
    DWORD ReplySize;
    int maxHops = 30;

    hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE) {
        
        PostMessageA(hMainWnd, WM_PING_RESULT, 0,
            (LPARAM)"[ERRO] Falha ao criar handle ICMP para tracert\r\n");
        SendTracertComplete();
        HeapFree(GetProcessHeap(), 0, target);
        return 1;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ReplySize);

    for (int ttl = 1; ttl <= maxHops && !bStopTracert; ttl++) {
        IP_OPTION_INFORMATION options = { 0 };
        options.Ttl = (BYTE)ttl;

        BOOL reachedDestination = FALSE;
        char hopIP[16] = "*";
        DWORD rtt = 0;

        for (int i = 0; i < 3 && !bStopTracert; i++) {
            if (ReplyBuffer) {
                DWORD status = IcmpSendEcho2(
                    hIcmpFile,
                    NULL,
                    NULL,
                    NULL,
                    inet_addr(target),
                    SendData,
                    sizeof(SendData),
                    &options,
                    ReplyBuffer,
                    ReplySize,
                    2000
                );

                if (status != 0) {
                    PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;

                    struct in_addr addr;
                    addr.S_un.S_addr = pEchoReply->Address;
                    strcpy_s(hopIP, sizeof(hopIP), inet_ntoa(addr));

                    rtt = pEchoReply->RoundTripTime;

                    if (pEchoReply->Status == IP_SUCCESS) {
                        reachedDestination = TRUE;
                    }
                }
            }

            Sleep(100);
        }

        SendTracertHop(ttl, hopIP, rtt);

        if (reachedDestination) {
            break;
        }
    }

    HeapFree(GetProcessHeap(), 0, ReplyBuffer);
    IcmpCloseHandle(hIcmpFile);
    HeapFree(GetProcessHeap(), 0, target);

    SendTracertComplete();
    return 0;
}

void RunTracert(const char* target) {
    bStopTracert = FALSE;

    size_t len = strlen(target) + 1;
    char* targetCopy = (char*)HeapAlloc(GetProcessHeap(), 0, len);
    if (targetCopy) {
        strcpy_s(targetCopy, len, target);
        hTracertThread = CreateThread(NULL, 0, TracertWorkerThread, targetCopy, 0, NULL);
    }
}

void StopTracert() {
    bStopTracert = TRUE;
    if (hTracertThread != NULL) {
        WaitForSingleObject(hTracertThread, 3000);
        CloseHandle(hTracertThread);
        hTracertThread = NULL;
    }
    SendTracertComplete();
}