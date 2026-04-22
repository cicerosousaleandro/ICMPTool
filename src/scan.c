#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#define WM_SCAN_RESULT (WM_USER + 5)
#define WM_SCAN_COMPLETE (WM_USER + 6)

extern HWND hMainWnd;

volatile BOOL bStopScan = FALSE;
extern HANDLE hScanThread;

void SendScanResult(const char* ip, const char* status) {
    if (!ip || !status) return;

    char msg[64];
    int written = sprintf_s(msg, sizeof(msg), "%-16s %s\r\n", ip, status);
    if (written <= 0) return;

    size_t len = strlen(msg) + 1;
    char* msgCopy = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
    if (msgCopy) {
        strcpy_s(msgCopy, len, msg);
        PostMessageA(hMainWnd, WM_SCAN_RESULT, 0, (LPARAM)msgCopy);
    }
}

BOOL GetLocalNetworkInfo(char* baseIP, int* lastOctetStart) {
    if (!baseIP || !lastOctetStart) return FALSE;

    PIP_ADAPTER_INFO pAdapterInfo = NULL;
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
    if (!pAdapterInfo) return FALSE;

    DWORD dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(ulOutBufLen);
        if (!pAdapterInfo) return FALSE;
        dwRetVal = GetAdaptersInfo(pAdapterInfo, &ulOutBufLen);
    }

    if (dwRetVal == NO_ERROR) {
        PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
        while (pAdapter) {
            if ((pAdapter->Type == MIB_IF_TYPE_ETHERNET ||
                pAdapter->Type == IF_TYPE_IEEE80211 ||
                pAdapter->Type == IF_TYPE_SOFTWARE_LOOPBACK) &&
                strlen(pAdapter->IpAddressList.IpAddress.String) > 0 &&
                strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {

                strncpy_s(baseIP, 32, pAdapter->IpAddressList.IpAddress.String, _TRUNCATE);
                char* lastDot = strrchr(baseIP, '.');
                if (lastDot) {
                    *lastDot = '\0';
                    *lastOctetStart = atoi(lastDot + 1);
                }
                else {
                    *lastOctetStart = 1;
                }

                free(pAdapterInfo);
                return TRUE;
            }
            pAdapter = pAdapter->Next;
        }
    }

    free(pAdapterInfo);
    return FALSE;
}

DWORD WINAPI ScanWorkerThread(LPVOID lpParam) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in targetAddr;
    char baseIP[32] = { 0 };
    int startOctet = 1;

    memset(&targetAddr, 0, sizeof(targetAddr));

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        SendScanResult("ERRO", "Falha ao iniciar Winsock");
        PostMessageA(hMainWnd, WM_SCAN_COMPLETE, 0, 0);
        return 1;
    }

    if (!GetLocalNetworkInfo(baseIP, &startOctet)) {
        SendScanResult("ERRO", "Rede local não detectada");
        WSACleanup();
        PostMessageA(hMainWnd, WM_SCAN_COMPLETE, 0, 0);
        return 1;
    }

    for (int i = 1; i <= 254 && !bStopScan; i++) {
        if (i == startOctet) continue;

        char fullIP[32];
        sprintf_s(fullIP, sizeof(fullIP), "%s.%d", baseIP, i);

        BOOL hostAlive = FALSE;

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock != INVALID_SOCKET) {
            DWORD timeout = 100;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

            targetAddr.sin_family = AF_INET;
            targetAddr.sin_port = htons(7);

            if (inet_pton(AF_INET, fullIP, &targetAddr.sin_addr) == 1) {
                sendto(sock, "x", 1, 0, (struct sockaddr*)&targetAddr, sizeof(targetAddr));

                char recvBuf[10];
                int ret = recvfrom(sock, recvBuf, 10, 0, NULL, NULL);

                if (ret >= 0 || WSAGetLastError() == WSAECONNRESET) {
                    hostAlive = TRUE;
                }
            }
            closesocket(sock);
            sock = INVALID_SOCKET;
        }

        if (!hostAlive && !bStopScan) {
            HANDLE hIcmp = IcmpCreateFile();
            if (hIcmp && hIcmp != INVALID_HANDLE_VALUE) {
                char icmpData[8] = "ICMPScan";
                DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(icmpData);
                LPVOID replyBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, replySize);

                if (replyBuffer) {
                    DWORD status = IcmpSendEcho(hIcmp, inet_addr(fullIP), icmpData,
                        sizeof(icmpData), NULL, replyBuffer,
                        replySize, 500);

                    if (status != 0) {
                        PICMP_ECHO_REPLY pReply = (PICMP_ECHO_REPLY)replyBuffer;
                        if (pReply && pReply->Status == IP_SUCCESS) {
                            hostAlive = TRUE;
                        }
                    }

                    HeapFree(GetProcessHeap(), 0, replyBuffer);
                }
                IcmpCloseHandle(hIcmp);
            }
        }

        if (hostAlive && !bStopScan) {
            SendScanResult(fullIP, "ATIVO");
        }

        Sleep(10);
    }

    if (sock != INVALID_SOCKET) closesocket(sock);
    WSACleanup();
    PostMessageA(hMainWnd, WM_SCAN_COMPLETE, 0, 0);
    return 0;
}

void RunNetworkScan(void) {
    bStopScan = FALSE;
    hScanThread = CreateThread(NULL, 0, ScanWorkerThread, NULL, 0, NULL);
}

void StopNetworkScan(void) {
    bStopScan = TRUE;
    if (hScanThread && hScanThread != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(hScanThread, 2000);
        CloseHandle(hScanThread);
        hScanThread = NULL;
    }
    PostMessageA(hMainWnd, WM_SCAN_COMPLETE, 0, 0);
}