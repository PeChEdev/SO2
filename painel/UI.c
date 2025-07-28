#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include "../arbitro/util.h"
#include "../arbitro/arbitro.c"
#include "resource.h"


LRESULT CALLBACK trataEventos(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI UpdateThread(HWND, UINT, WPARAM, LPARAM);


TCHAR szProgName[] = TEXT("Base");


#define TAM 100
#define MAX_LETRAS_DISPLAY 6
#define PLAYER_LIST_COMPRIMENTO 200 
#define PADDING 10

typedef struct {
    HWND hwnd;
    HANDLE hMapFile;
    SDATA* pSharedData;
    HANDLE hSharedMutex;
    HANDLE hSharedEvent;
    TCHAR displayLetras[TAM + 1];
    CRITICAL_SECTION csDisplayLetras;
    HANDLE hUpdateThread;
    BOOL continua; 
    RECT clientRectDim; 
    int tamanhodaLista;
} TDATA_UI;

int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int nCmdShow) {
    HWND hWnd;          
    MSG lpMsg;          
    WNDCLASSEX wcApp;   

    wcApp.cbSize = sizeof(WNDCLASSEX);
    wcApp.hInstance = hInst;
    wcApp.lpszClassName = szProgName;  
    wcApp.lpfnWndProc = trataEventos;  
    wcApp.style = CS_HREDRAW | CS_VREDRAW;  
    wcApp.hIcon = LoadIcon(NULL, IDI_SHIELD); 
    wcApp.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wcApp.hCursor = LoadCursor(NULL, IDC_ARROW); 
    wcApp.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);  
    wcApp.cbClsExtra = 0;  
    wcApp.cbWndExtra = sizeof(TDATA_UI*);
    wcApp.hbrBackground = CreateSolidBrush(RGB(57, 77, 250)); 

    if (!RegisterClassEx(&wcApp))
        return(0);

    hWnd = CreateWindow(
        szProgName,  
        TEXT("Jogo Palavras"),  
        WS_OVERLAPPEDWINDOW,	
        CW_USEDEFAULT,  
        CW_USEDEFAULT,  
        CW_USEDEFAULT,  
        CW_USEDEFAULT,  
        (HWND)HWND_DESKTOP,	
        (HMENU)NULL,  
        (HINSTANCE)hInst,  
        0);

    ShowWindow(hWnd, nCmdShow); 
    UpdateWindow(hWnd);  

    while (GetMessage(&lpMsg, NULL, 0, 0) > 0) {
        TranslateMessage(&lpMsg); 
        DispatchMessage(&lpMsg); 
    }


    return (int)lpMsg.wParam;  
}


LRESULT CALLBACK trataDlg(HWND hDlg, UINT messg, WPARAM wParam, LPARAM lParam) {
    TCHAR str[100];
    TDATA_UI* ptd;
    HWND hWndParent;
    int novaAltura;

    hWndParent = GetParent(hDlg);
    ptd = (TDATA_UI*)GetWindowLongPtr(hWndParent, 0);

    switch (messg) {
    case WM_INITDIALOG:
        if (ptd) {
            _itot_s(ptd->tamanhodaLista, str, sizeof(str) / sizeof(TCHAR), 10);
            SetDlgItemText(hDlg, IDC_EDIT1, str);
        }
        else {
            SetDlgItemText(hDlg, IDC_EDIT1, _T("250"));
        }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            if (ptd) {
                GetDlgItemText(hDlg, IDC_EDIT1, str, sizeof(str) / sizeof(TCHAR));
                novaAltura = _ttoi(str);

                int alturaTitulo = 30; 
                int espacoVerticalTotal = ptd->clientRectDim.bottom - ptd->clientRectDim.top;
                int maxAlturaConteudoLista = espacoVerticalTotal - (2 * PADDING) - alturaTitulo - (PADDING / 2);

                if (novaAltura > 20 && novaAltura <= maxAlturaConteudoLista) { 
                    ptd->tamanhodaLista = novaAltura; 
                    InvalidateRect(hWndParent, NULL, TRUE);
                    EndDialog(hDlg, IDOK);
                }
            }
            else {
                EndDialog(hDlg, IDCANCEL);
            }
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return TRUE;
    }
    return FALSE;
}

DWORD WINAPI AtualizaThread(LPVOID lpParam) {
    TDATA_UI* ptd = (TDATA_UI*)lpParam;
    DWORD waitResult;

    while (ptd->continua) {
        waitResult = WaitForSingleObject(ptd->hSharedEvent, 200);

        if (waitResult == WAIT_OBJECT_0) {
            if (WaitForSingleObject(ptd->hSharedMutex, INFINITE) == WAIT_OBJECT_0) {
                EnterCriticalSection(&ptd->csDisplayLetras);

                for (int i = 0; i < MAX_LETRAS_DISPLAY; ++i) {
                    if (i < TAM) { 
                        if (ptd->pSharedData->letras[i] >= 32 && ptd->pSharedData->letras[i] <= 126) { 
                            MultiByteToWideChar(CP_ACP, 0, &ptd->pSharedData->letras[i], 1, &ptd->displayLetras[i], 1);
                        }
                        else {
                            ptd->displayLetras[i] = _T(' '); 
                        }
                    }
                    else {
                        ptd->displayLetras[i] = _T(' ');
                    }
                }
                ptd->displayLetras[MAX_LETRAS_DISPLAY] = _T('\0');

                LeaveCriticalSection(&ptd->csDisplayLetras);
                ReleaseMutex(ptd->hSharedMutex);

                InvalidateRect(ptd->hwnd, NULL, TRUE);
            }
        }
        else if (waitResult == WAIT_TIMEOUT) {
            continue;
        }
    }
    return 0;
}

LRESULT CALLBACK trataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
    HDC hdc;
    PAINTSTRUCT ps;
    TDATA_UI* ptd;
    RECT listaJogadores;

    switch (messg) {
    case WM_CREATE:
        ptd = (TDATA_UI*)malloc(sizeof(TDATA_UI));
        SetWindowLongPtr(hWnd, 0, (LONG_PTR)ptd);
        ptd->hwnd = hWnd;
        ptd->continua = FALSE;
        ptd->hMapFile = NULL;
        ptd->pSharedData = NULL;
        ptd->hSharedMutex = NULL;
        ptd->hSharedEvent = NULL;
        ptd->hUpdateThread = NULL;
        ptd->tamanhodaLista = PLAYER_LIST_COMPRIMENTO;

        GetClientRect(hWnd, &ptd->clientRectDim);

        for (int i = 0; i < MAX_LETRAS_DISPLAY; ++i) {
            ptd->displayLetras[i] = _T(' '); 
        }
        ptd->displayLetras[MAX_LETRAS_DISPLAY] = _T('\0');

        InitializeCriticalSection(&ptd->csDisplayLetras);

        ptd->hMapFile = OpenFileMapping(FILE_MAP_READ, FALSE, NOME_MAPA);
        if (ptd->hMapFile == NULL) {
			_tprintf_s(_T("Não foi possível abrir o mapeamento de ficheiro (OpenFileMapping). Erro: %d\n"), GetLastError());
            break;
        }

        ptd->pSharedData = (SDATA*)MapViewOfFile(ptd->hMapFile, FILE_MAP_READ, 0, 0, sizeof(SDATA));
        if (ptd->pSharedData == NULL) {
			_tprintf_s(_T("Não foi possível mapear a vista do ficheiro (MapViewOfFile). Erro: %d\n"), GetLastError());
            break;
        }

        ptd->hSharedMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, NOME_MUTEX);
        if (ptd->hSharedMutex == NULL) {
			_tprintf_s(_T("Não foi possível abrir o Mutex partilhado (OpenMutex). Erro: %d\n"), GetLastError());
            break;
        }

        ptd->hSharedEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, NOME_EVENTO);
        if (ptd->hSharedEvent == NULL) {
			_tprintf_s(_T("Não foi possível abrir o Evento partilhado (OpenEvent). Erro: %d\n"), GetLastError());
            break;
        }

        ptd->continua = TRUE;
        ptd->hUpdateThread = CreateThread(NULL, 0, AtualizaThread, ptd, 0, NULL);
        if (ptd->hUpdateThread == NULL) {
			_tprintf_s(_T("Não foi possível criar a thread de atualização (CreateThread). Erro: %d\n"), GetLastError());
            ptd->continua = FALSE;
        }
        break;

    case WM_SIZE:
        ptd = (TDATA_UI*)GetWindowLongPtr(hWnd, 0);
        if (ptd) {
            GetClientRect(hWnd, &ptd->clientRectDim);
            InvalidateRect(hWnd, NULL, TRUE);
        }
        break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        break;

    case WM_PAINT:  
           ptd = (TDATA_UI*)GetWindowLongPtr(hWnd, 0);  
           hdc = BeginPaint(hWnd, &ps);  

           if (ptd) {
               SetBkMode(hdc, TRANSPARENT);
               SetTextColor(hdc, RGB(255, 255, 255));

               RECT playerListRect; 
               RECT titleRect;      
               titleRect.right = ptd->clientRectDim.right - PADDING;
               titleRect.left = titleRect.right - PLAYER_LIST_COMPRIMENTO; 

               playerListRect.left = titleRect.left;
               playerListRect.right = titleRect.right;

               int alturaTituloLista = 30; 
               titleRect.top = ptd->clientRectDim.top + PADDING;
               titleRect.bottom = titleRect.top + alturaTituloLista;

               TCHAR szPlayerListTitle[] = TEXT("Lista de Jogadores");
               DrawText(hdc, szPlayerListTitle, -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);


               playerListRect.top = titleRect.bottom + PADDING / 2; 

               playerListRect.bottom = playerListRect.top + ptd->tamanhodaLista;


               if (playerListRect.bottom > ptd->clientRectDim.bottom - PADDING) {
                   playerListRect.bottom = ptd->clientRectDim.bottom - PADDING;
               }
 
               if (playerListRect.top < playerListRect.bottom) {
                   Rectangle(hdc, playerListRect.left, playerListRect.top, playerListRect.right, playerListRect.bottom);
               }
           }

           if (ptd && ptd->continua) {  
               TCHAR localDisplayCopy[MAX_LETRAS_DISPLAY + 1];  

               EnterCriticalSection(&ptd->csDisplayLetras);  
               StringCchCopy(localDisplayCopy, MAX_LETRAS_DISPLAY + 1, ptd->displayLetras);  
               LeaveCriticalSection(&ptd->csDisplayLetras);  

               SetTextColor(hdc, RGB(255, 255, 255));  
               SetBkMode(hdc, TRANSPARENT);  

               SIZE tamanho;  
               GetTextExtentPoint32(hdc, localDisplayCopy, MAX_LETRAS_DISPLAY, &tamanho);
               int xPos = (ptd->clientRectDim.right - tamanho.cx) / 2;
               int yPos = (ptd->clientRectDim.bottom - tamanho.cy) / 2;

               for (DWORD i = 0; i < MAX_LETRAS_DISPLAY; ++i) {  
                   TextOut(hdc, xPos + (i * 20), yPos, &localDisplayCopy[i], 1);  
               }  

           }  

           EndPaint(hWnd, &ps);  
           break;

    case WM_COMMAND:
        ptd = (TDATA_UI*)GetWindowLongPtr(hWnd, 0);
        if (HIWORD(wParam) == 0) {
            switch (LOWORD(wParam)) {
            case ID_ACERCA:
                MessageBox(hWnd, _T("Trabalho Realizado por:\n Diogo Oliveira - 2021146037\nTomás Laranjeira - 2021135060"), _T("Sobre"), MB_OK | MB_ICONINFORMATION);
                break;
            case ID_FICHEIRO_TOP10:
				DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_DIALOG1), hWnd, trataDlg);
                break;
            case ID_FICHEIRO_SAIR:
                PostMessage(hWnd, WM_CLOSE, 0, 0);
                break;
            }
        }
        break;

    case WM_CLOSE:
        if (MessageBox(hWnd, _T("Queres terminar a aplicação?"), _T("Fechar Aplicação"), MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
            DestroyWindow(hWnd);
        }
        break;

    case WM_DESTROY:
        ptd = (TDATA_UI*)GetWindowLongPtr(hWnd, 0);
        if (ptd) {
            if (ptd->continua && ptd->hUpdateThread) {
                ptd->continua = FALSE;
                if (ptd->hSharedEvent) SetEvent(ptd->hSharedEvent);
                WaitForSingleObject(ptd->hUpdateThread, INFINITE);
                CloseHandle(ptd->hUpdateThread);
            }

            DeleteCriticalSection(&ptd->csDisplayLetras);

            if (ptd->hSharedEvent) CloseHandle(ptd->hSharedEvent);
            if (ptd->hSharedMutex) CloseHandle(ptd->hSharedMutex);
            if (ptd->pSharedData) UnmapViewOfFile(ptd->pSharedData);
            if (ptd->hMapFile) CloseHandle(ptd->hMapFile);

            free(ptd);
            SetWindowLongPtr(hWnd, 0, (LONG_PTR)NULL);
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, messg, wParam, lParam);
    }
    return 0;
}