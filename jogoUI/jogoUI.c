#include "../arbitro/util.h"

DWORD WINAPI threadTeclado(LPVOID lParam) {
	TDATAJogo* pjogo = (TDATAJogo*)lParam;
	TCHAR input[100];
	BOOL ret;
	DWORD bytesWritten;

	OVERLAPPED ov;
	HANDLE hEv = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEv == NULL) {
		_tprintf(_T("Erro a criar o evento\n"));
		pjogo->continua = FALSE;
		SetEvent(pjogo->hEvent);
		ExitThread(1);
	}
	ZeroMemory(&ov, sizeof(OVERLAPPED));
	ov.hEvent = hEv;

	ret = WriteFile(pjogo->hPipe, pjogo->nomeUser, (DWORD)_tcslen(pjogo->nomeUser) * sizeof(TCHAR), &bytesWritten, &ov);
	if (!ret && GetLastError() != ERROR_IO_PENDING) {
		_tprintf_s(_T("[ERRO] %d (%d bytes)... (WriteFile)\n"), ret, bytesWritten);
		pjogo->continua = FALSE;
		SetEvent(pjogo->hEvent);
	}
	if (GetLastError() == ERROR_IO_PENDING) {
		WaitForSingleObject(hEv, INFINITE); //esperar pelo fim da operaçao
		GetOverlappedResult(pjogo->hPipe, &ov, &bytesWritten, FALSE); //obter o resultado da operaçao
	}
	_tprintf_s(_T("Enviei %d bytes: '%s'... (WriteFile)\n"), bytesWritten, pjogo->nomeUser);

	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE waitHandles[2] = { hStdin, pjogo->hEventStop };

	while (pjogo->continua) {
		_tprintf(_T("\nInsere uma palavra ou comando: "));
		fflush(stdout);

		DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

		if (!pjogo->continua)
			break;

		if (waitResult == WAIT_OBJECT_0) {
			if (_fgetts(input, 100, stdin) == NULL) {
				_tprintf(_T("Erro a ler do teclado\n"));
				pjogo->continua = FALSE;
				SetEvent(pjogo->hEvent);
				break;
			}

			input[_tcslen(input) - 1] = _T('\0');

			if (_tcscmp(input, _T(":sair")) == 0) {
				_tprintf(_T("A sair...\n"));
				pjogo->continua = FALSE;
				SetEvent(pjogo->hEvent);
				break;
			}
			else if (_tcslen(input) == 0) {
				_tprintf(_T("Comando vazio. Tente novamente.\n"));
				continue;
			}
			else {
				ZeroMemory(&ov, sizeof(OVERLAPPED));
				ov.hEvent = hEv;

				ret = WriteFile(pjogo->hPipe, input, (DWORD)_tcslen(input) * sizeof(TCHAR), &bytesWritten, &ov);
				if (!ret && GetLastError() != ERROR_IO_PENDING) {
					_tprintf_s(_T("[ERRO] %d (%d bytes)... (WriteFile)\n"), ret, bytesWritten);
					pjogo->continua = FALSE;
					SetEvent(pjogo->hEvent);
					break;
				}

				if (GetLastError() == ERROR_IO_PENDING) {
					if (WaitForSingleObject(hEv, INFINITE) == WAIT_OBJECT_0) {
						GetOverlappedResult(pjogo->hPipe, &ov, &bytesWritten, FALSE);
					}
					else {
						_tprintf(_T("[ERRO] %d (%d bytes)... (WaitForSingleObject)\n"), ret, bytesWritten);
						pjogo->continua = FALSE;
						SetEvent(pjogo->hEvent);
						break;
					}
				}
				_tprintf_s(_T("Enviei %d bytes: '%s'... (WriteFile)\n"), bytesWritten, input);
			}
		}
		else if (waitResult == WAIT_OBJECT_0 + 1) {
			break;
		}
		else {
			_tprintf(_T("[ERRO] %d (%d bytes)... (WaitForMultipleObjects)\n"), ret, bytesWritten);
			pjogo->continua = FALSE;
			SetEvent(pjogo->hEvent);
			break;
		}
	}
	if (pjogo->hPipe != INVALID_HANDLE_VALUE) {

		CloseHandle(pjogo->hPipe);
		pjogo->hPipe = INVALID_HANDLE_VALUE;
	}
	CloseHandle(hEv);
	ExitThread(0);
}

DWORD WINAPI recebeLetras(LPVOID lParam) {
	TDATAJogo* pjogo = (TDATAJogo*)lParam;

	while (pjogo->continua) {
		HANDLE waitHandles[2] = { pjogo->hEventStop, pjogo->hEvent };
		DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (waitResult == WAIT_OBJECT_0) {
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
	ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[]) {
	HANDLE hMap, hEvent, hMutex, hThreadT, hThreadL;
	OVERLAPPED ov;
	HANDLE hEv;
	SDATA* pData;
	BOOL ret;
	DWORD bytesRead;
	TCHAR buf[256];

	TDATAJogo jogo;

#ifdef UNICODE
	(void) _setmode(_fileno(stdin), _O_WTEXT);
	(void)_setmode(_fileno(stdout), _O_WTEXT);
	(void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

	if (argc != 2) {
		_tprintf(_T("Uso: %s <nome do jogador>\n"), argv[0]);
		return 1;
	}

	hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, NOME_MAPA);
	if (hMap == NULL)
	{
		_tprintf_s(_T("Erro a abrir o mapeamento de ficheiro\n"));
		return 1;
	}
	pData = (SDATA*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, TAM * sizeof(TCHAR));
	if (pData == NULL)
	{
		_tprintf_s(_T("Erro a mapear o ficheiro\n"));
		CloseHandle(hMap);
		return 1;
	}

	hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, NOME_MUTEX);
	if (hMutex == NULL)
	{
		_tprintf_s(_T("Erro a abrir o mutex\n"));
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, NOME_EVENTO);
	if (hEvent == NULL)
	{
		_tprintf_s(_T("Erro a abrir o evento\n"));
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	jogo.hEventStop = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (jogo.hEventStop == NULL) {
		_tprintf(_T("Erro a criar o evento\n"));
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	_tprintf_s(_T("Jogador conectado. À espera de letras :)\n"));

	_tcscpy_s(jogo.nomeUser, USERTAM, argv[1]);
	jogo.continua = TRUE;
	jogo.pData = pData;
	jogo.hEvent = hEvent;
	jogo.hMutex = hMutex;

	if (!WaitNamedPipe(NOME_PIPE, NMPWAIT_WAIT_FOREVER)) {
		CloseHandle(jogo.hEventStop);
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}
	jogo.hPipe = CreateFile(NOME_PIPE, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (jogo.hPipe == INVALID_HANDLE_VALUE) {
		_tprintf(_T("[ERRO] Ligar ao pipe '%s'! (CreateFile)\n"), NOME_PIPE);
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	DWORD modo = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState(jogo.hPipe, &modo, NULL, NULL);

	hThreadT = CreateThread(NULL, 0, threadTeclado, (LPVOID)&jogo, 0, NULL);
	if (hThreadT == NULL) {
		_tprintf_s(_T("Erro a criar a thread do jogador\n"));
		CloseHandle(jogo.hPipe);
		CloseHandle(jogo.hEventStop);
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	hThreadL = CreateThread(NULL, 0, recebeLetras, (LPVOID)&jogo, 0, NULL);
	if (hThreadL == NULL) {
		_tprintf_s(_T("Erro a criar a thread de letras\n"));
		jogo.continua = FALSE;
		SetEvent(jogo.hEventStop);
		WaitForSingleObject(hThreadT, INFINITE);
		CloseHandle(hThreadT);
		if (jogo.hPipe != INVALID_HANDLE_VALUE) {
			CloseHandle(jogo.hPipe);
		}
		CloseHandle(jogo.hEventStop);
		CloseHandle(hThreadT);
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}

	hEv = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEv == NULL) {
		_tprintf(_T("Erro a criar o evento\n"));
		jogo.continua = FALSE;
		SetEvent(jogo.hEvent);
		SetEvent(jogo.hEventStop);
		WaitForSingleObject(hThreadT, INFINITE);
		CloseHandle(hThreadT);
		WaitForSingleObject(hThreadL, INFINITE);
		CloseHandle(hThreadL);

		CloseHandle(jogo.hPipe);
		CloseHandle(jogo.hEventStop);
		CloseHandle(hEvent);
		CloseHandle(hMutex);
		UnmapViewOfFile(pData);
		CloseHandle(hMap);
		return 1;
	}
	while (jogo.continua) {
		ZeroMemory(&ov, sizeof(OVERLAPPED));
		ov.hEvent = hEv;

		if (jogo.hPipe == INVALID_HANDLE_VALUE) {
			_tprintf(_T("[ERRO] Pipe '%s' foi fechado! (CreateFile)\n"), NOME_PIPE);
			jogo.continua = FALSE;
			SetEvent(jogo.hEvent);
			break;
		}

		ret = ReadFile(jogo.hPipe, buf, sizeof(buf), &bytesRead, &ov);
		if (!ret && GetLastError() != ERROR_IO_PENDING) {
			_tprintf_s(_T("[ERRO] %d (%d bytes)... (ReadFile)\n"), ret, bytesRead);
			jogo.continua = FALSE;
			SetEvent(jogo.hEvent);
			SetEvent(jogo.hEventStop);
			break;
		}
		if (GetLastError() == ERROR_IO_PENDING) {
			HANDLE waitHandles[2] = { hEv, jogo.hEventStop };

			DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

			if (waitResult == WAIT_OBJECT_0) {
				if (!GetOverlappedResult(jogo.hPipe, &ov, &bytesRead, FALSE)) {
					_tprintf_s(_T("[ERRO] %d (%d bytes)... (GetOverlappedResult)\n"), ret, bytesRead);
					jogo.continua = FALSE;
					SetEvent(jogo.hEvent);
					SetEvent(jogo.hEventStop);
					break;
				}
			}
			else if (waitResult == WAIT_OBJECT_0 + 1) {
				_tprintf(_T("A sair...\n"));
				jogo.continua = FALSE;
				SetEvent(jogo.hEvent);
				break;
			}
			else {
				_tprintf(_T("[ERRO] %d (%d bytes)... (WaitForMultipleObjects)\n"), ret, bytesRead);
				jogo.continua = FALSE;
				SetEvent(jogo.hEvent);
				SetEvent(jogo.hEventStop);
				break;
			}
		}

		if (!jogo.continua)
			break;

		if (bytesRead == 0 && ret) {
			_tprintf(_T("[ERRO] Pipe '%s' foi fechado! (ReadFile)\n"), NOME_PIPE);
			jogo.continua = FALSE;
			SetEvent(jogo.hEvent);
			SetEvent(jogo.hEventStop);
			break;
		}

		buf[bytesRead / sizeof(TCHAR)] = '\0';

		if (_tcscmp(buf, _T("sair")) == 0) {
			_tprintf(_T("A sair...\n"));
			jogo.continua = FALSE;
			SetEvent(jogo.hEvent);
			SetEvent(jogo.hEventStop);
			break;
		}
		else if (_tcscmp(buf, _T("existe")) == 0) {
			_tprintf(_T("Já existe um jogador com o username %s\n"), jogo.nomeUser);
			jogo.continua = FALSE;
			SetEvent(jogo.hEvent);
			SetEvent(jogo.hEventStop);
			break;
		}
		else
			_tprintf_s(_T("[LEITOR] Recebi %d bytes: '%s'... (ReadFile)\n"), bytesRead, buf);
	}

	WaitForSingleObject(hThreadT, INFINITE);
	CloseHandle(hThreadT);

	WaitForSingleObject(hThreadL, INFINITE);
	CloseHandle(hThreadL);

	CloseHandle(hEv);
	if (jogo.hPipe != INVALID_HANDLE_VALUE) {
		CloseHandle(jogo.hPipe);
	}
	CloseHandle(jogo.hEventStop);
	UnmapViewOfFile(pData);
	CloseHandle(hMap);
	CloseHandle(hEvent);
	CloseHandle(hMutex);
}
