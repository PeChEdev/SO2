#include "../arbitro/util.h"

int contemLetrasDisponiveis(const TCHAR* palavra, TCHAR* letras, int numLetras) {
    for (int i = 0; palavra[i] != '\0'; i++) {
        BOOL encontrada = FALSE;
        for (int j = 0; j < numLetras; j++) {
            if (palavra[i] == letras[j]) {
                encontrada = TRUE;
                break;
            }
        }
        if (!encontrada) return 0;
    }
    return 1;
}

DWORD WINAPI recebeLetras(LPVOID lParam) {
    TBOTDATA* pjogo = (TBOTDATA*)lParam;

    while (pjogo->continua) {
        HANDLE waitHandles[2] = { pjogo->hEventStop, pjogo->hEvent };
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0) {
            _tprintf(_T("A encerrar thread de letras\n"));
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            WaitForSingleObject(pjogo->hMutex, INFINITE);

            _tprintf(_T("Letras recebidas: "));
            for (int i = 0; i < TAM; i++) {
                TCHAR c = pjogo->pData->letras[i];
                if (c >= 'A' && c <= 'Z') {
                    _tprintf(_T("%c "), c);
                }
            }
            _tprintf(_T("\n"));

            ReleaseMutex(pjogo->hMutex);
            ResetEvent(pjogo->hEvent);
        }
        else {
            _tprintf(_T("[ERRO] %d... (WaitForMultipleObjects)\n"), GetLastError());
            pjogo->continua = FALSE;
            SetEvent(pjogo->hEvent);
            break;
        }
    }
    _tprintf(_T("[BOT] Thread de letras encerrada.\n"));
    ExitThread(0);
}

DWORD WINAPI threadAdivinhaPalavras(LPVOID lParam) {
    TBOTDATA* pbot = (TBOTDATA*)lParam;
    TCHAR* palavras[1000];
    int nPalavras = 0;
    TCHAR linha[TAM_PALAVRA];
    FILE* f;
    BOOL ret;
    DWORD bytesWritten;

    _tfopen_s(&f, _T("dicionarioBot.txt"), _T("r"));
    if (f == NULL) {
        _tprintf(_T("[BOT] Erro a abrir o ficheiro de dicionário\n"));
        ExitThread(1);
    }
    while (_fgetts(linha, TAM_PALAVRA, f) != NULL && nPalavras < 1000) {
        size_t len = _tcslen(linha);
        if (len > 0 && linha[len - 1] == '\n')
            linha[len - 1] = '\0';
        palavras[nPalavras] = _tcsdup(linha);
        nPalavras++;
    }
    fclose(f);

    for (DWORD i = 0; i < nPalavras; i++) {
        _tprintf(_T("[BOT] Palavra carregada: %s\n"), palavras[i]);
    }


    OVERLAPPED ov;
    HANDLE hEv = CreateEvent(NULL, TRUE, FALSE, NULL);

    while (pbot->continua) {
        Sleep(pbot->tempoReacao * 1000);


        _tprintf(_T("[BOT] Letras disponíveis: "));
        for (int i = 0; i < TAM && pbot->pData->letras[i] != '\0'; i++) {
            _tprintf(_T("%c "), pbot->pData->letras[i]);
        }
        _tprintf(_T("\n"));

        for (int i = 0; i < nPalavras && pbot->continua; i++) {
            if (contemLetrasDisponiveis(palavras[i], pbot->pData->letras, TAM)) {
                _tprintf(_T("[BOT] Palavra encontrada: %s\n"), palavras[i]);

                ZeroMemory(&ov, sizeof(OVERLAPPED));
                ov.hEvent = hEv;
                ret = WriteFile(pbot->hPipe, palavras[i], (DWORD)_tcslen(palavras[i]) * sizeof(TCHAR), &bytesWritten, &ov);
                if (!ret && GetLastError() != ERROR_IO_PENDING) {
                    _tprintf(_T("[BOT] Erro ao enviar palavra\n"));
                    pbot->continua = FALSE;
                    break;
                }
                if (GetLastError() == ERROR_IO_PENDING) {
                    WaitForSingleObject(hEv, INFINITE);
                    GetOverlappedResult(pbot->hPipe, &ov, &bytesWritten, FALSE);
                }
                else if (GetLastError() != ERROR_SUCCESS) {
                    pbot->continua = FALSE;
                    SetEvent(pbot->hEventStop);
                    break;
                }
                _tprintf(_T("[BOT] Enviei %d bytes: '%s'\n"), bytesWritten, palavras[i]);
                break;
            }
        }


        for (int i = 0; i < nPalavras; i++) free(palavras[i]);
        CloseHandle(hEv);
        ExitThread(0);
    }
}

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hMap, hEvent, hMutex, hThreadLetras, hThreadAdivinha;
    SDATA* pData;
    TBOTDATA bot;
    DWORD modo;
    OVERLAPPED ov;
    HANDLE hEv;
    BOOL ret;
    DWORD bytesWritten;

#ifdef UNICODE
    (void) _setmode(_fileno(stdin), _O_WTEXT);
    (void)_setmode(_fileno(stdout), _O_WTEXT);
    (void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

    if (argc != 3) {
        _tprintf(_T("Uso: %s <username> <tempoReacao>\n"), argv[0]);
        return 1;
    }

    _tcscpy_s(bot.nomeUser, USERTAM, argv[1]);
    bot.tempoReacao = _tstoi(argv[2]);
    bot.continua = TRUE;

    hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, NOME_MAPA);
    if (hMap == NULL) {
        _tprintf(_T("Erro a abrir o mapeamento de ficheiro\n"));
        return 1;
    }
    pData = (SDATA*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, TAM * sizeof(TCHAR));
    if (pData == NULL) {
        _tprintf(_T("Erro a mapear o ficheiro\n"));
        CloseHandle(hMap);
        return 1;
    }

    hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, NOME_MUTEX);
    if (hMutex == NULL) {
        _tprintf(_T("Erro a abrir o mutex\n"));
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, NOME_EVENTO);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n"));
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    bot.hEventStop = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (bot.hEventStop == NULL) {
        _tprintf(_T("Erro a criar o evento\n"));
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    bot.pData = pData;
    bot.hEvent = hEvent;
    bot.hMutex = hMutex;

    if (!WaitNamedPipe(NOME_PIPE, NMPWAIT_WAIT_FOREVER)) {
        CloseHandle(bot.hEventStop);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }
    bot.hPipe = CreateFile(NOME_PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (bot.hPipe == INVALID_HANDLE_VALUE) {
        _tprintf(_T("[ERRO] Ligar ao pipe '%s'! (CreateFile)\n"), NOME_PIPE);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    modo = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(bot.hPipe, &modo, NULL, NULL);

    hEv = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (hEv == NULL) {
        _tprintf(_T("Erro a criar o evento\n"));
        CloseHandle(bot.hPipe);
        CloseHandle(bot.hEventStop);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    ZeroMemory(&ov, sizeof(OVERLAPPED));
    ov.hEvent = hEv;
    ret = WriteFile(bot.hPipe, bot.nomeUser, (DWORD)_tcslen(bot.nomeUser) * sizeof(TCHAR), &bytesWritten, &ov);
    if (!ret && GetLastError() != ERROR_IO_PENDING) {
        _tprintf(_T("[BOT] Erro ao enviar nome\n"));
        CloseHandle(hEv);
        CloseHandle(bot.hPipe);
        CloseHandle(bot.hEventStop);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }
    if (GetLastError() == ERROR_IO_PENDING) {
        WaitForSingleObject(hEv, INFINITE);
        GetOverlappedResult(bot.hPipe, &ov, &bytesWritten, FALSE);
    }
    _tprintf(_T("[BOT] Bot '%s' iniciado com %d segundos de reação.\n"), bot.nomeUser, bot.tempoReacao);

    hThreadLetras = CreateThread(NULL, 0, recebeLetras, (LPVOID)&bot, 0, NULL);
    if (hThreadLetras == NULL) {
        _tprintf(_T("Erro a criar a thread de letras\n"));
        CloseHandle(hEv);
        CloseHandle(bot.hPipe);
        CloseHandle(bot.hEventStop);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }
    hThreadAdivinha = CreateThread(NULL, 0, threadAdivinhaPalavras, (LPVOID)&bot, 0, NULL);
    if (hThreadAdivinha == NULL) {
        _tprintf(_T("Erro a criar a thread de adivinha\n"));
        CloseHandle(hThreadLetras);
        CloseHandle(hEv);
        CloseHandle(bot.hPipe);
        CloseHandle(bot.hEventStop);
        CloseHandle(hEvent);
        CloseHandle(hMutex);
        UnmapViewOfFile(pData);
        CloseHandle(hMap);
        return 1;
    }

    WaitForSingleObject(hThreadLetras, INFINITE);
    WaitForSingleObject(hThreadAdivinha, INFINITE);

    CloseHandle(hThreadLetras);
    CloseHandle(hThreadAdivinha);
    CloseHandle(hEv);
    if (bot.hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(bot.hPipe);
    }
    CloseHandle(bot.hEventStop);
    UnmapViewOfFile(pData);
    CloseHandle(hMap);
    CloseHandle(hEvent);
    CloseHandle(hMutex);
    return 0;
}