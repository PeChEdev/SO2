#include "util.h"

DWORD WINAPI ThreadLetras(LPVOID lpParam) {
	JOGO* jogo = (JOGO*)lpParam;
	int index = 0;

	while (jogo->continua) {
		if (jogo->numLetrasValidas == 0) {
			_tprintf(_T("[ThreadLetras] Nenhuma letra válida carregada.\n"));
			Sleep(jogo->ritmo * 1000);
			continue;
		}

		int prob = rand() % jogo->numLetrasValidas;
		TCHAR letra = jogo->letrasValidas[prob];

		WaitForSingleObject(jogo->hMutex, INFINITE);
		jogo->pData->letras[index] = letra;
		index = (index + 1) % jogo->maxLetras;

		SetEvent(jogo->hEvent);
		ReleaseMutex(jogo->hMutex);

		Sleep(jogo->ritmo * 1000);
	}

	ExitThread(0);
}


void CarregarPalavras(JOGO* jogo) {
	FILE* f;
	TCHAR buffer[TAM_PALAVRA];
	int i = 0;
	BOOL letrasUsadas[26] = { FALSE };

	_tfopen_s(&f, _T("dicionario.txt"), _T("r"));
	if (f == NULL) {
		_tprintf(_T("Erro a abrir o ficheiro de palavras\n"));
		return;
	}

	while (i < MAX_PALAVRA && _fgetts(buffer, TAM_PALAVRA, f) != NULL) {
		DWORD len = _tcslen(buffer);
		if (len > 0 && buffer[len - 1] == _T('\n')) {
			buffer[len - 1] = _T('\0');
		}

		_tcscpy_s(jogo->palavras[i], TAM_PALAVRA, buffer);

		for (DWORD j = 0; j < _tcslen(buffer); j++) {
			TCHAR c = _totupper(buffer[j]);
			if (c >= 'A' && c <= 'Z') {
				letrasUsadas[c - 'A'] = TRUE;
			}
		}
		i++;
	}
	jogo->numPalavras = i;
	fclose(f);

	jogo->numLetrasValidas = 0;
	for (int k = 0; k < 26; k++) {
		if (letrasUsadas[k]) {
			jogo->letrasValidas[jogo->numLetrasValidas++] = 'A' + k;
		}
	}
}

DWORD WINAPI leTeclado(LPVOID data) {
	JOGO* pjogo = (JOGO*)data;
	TCHAR comando[100];
	TCHAR* context = NULL;
	TCHAR* token = NULL;
	TCHAR nome[USERTAM];

	while (pjogo->continua) {
		_fgetts(comando, 100, stdin);
		comando[_tcslen(comando) - 1] = _T('\0');
		token = _tcstok_s(comando, _T(" "), &context);

		if (_tcscmp(token, _T("encerrar")) == 0)
		{
			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL)
			{
				_tprintf(_T("Demasiados argumentos\nUtilize 'encerrar'\n"));
				continue;
			}
			WaitForSingleObject(pjogo->hMutex, INFINITE);
			pjogo->continua = FALSE;
			SetEvent(pjogo->hEventStop);
			ReleaseMutex(pjogo->hMutex);
			break;
		}
		else if (_tcscmp(token, _T("lista")) == 0)
		{

			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL)
			{
				_tprintf(_T("Demasiados argumentos\nUtilize 'lista'\n"));
				continue;
			}
			_tprintf(_T("Lista de jogadores:\n"));
			WaitForSingleObject(pjogo->hMutex, INFINITE);
			for (int i = 0; i < pjogo->numJogadores; i++)
			{
				_tprintf(_T("%s - %0.1f pontos\n"), pjogo->jogadores[i].nomeUser, pjogo->jogadores[i].pontos);
			}
			ReleaseMutex(pjogo->hMutex);
		}
		else if (_tcscmp(token, _T("excluir")) == 0)
		{
			token = _tcstok_s(NULL, _T(" "), &context);
			if (token == NULL)
			{
				_tprintf(_T("Falta de argumentos\nUtilize 'excluir <nome>'\n"));
				continue;
			}

			_tcscpy_s(nome, USERTAM, token);

			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL)
			{
				_tprintf(_T("Demasiados argumentos\nUtilize 'excluir <nome>'\n"));
				continue;
			}

			WaitForSingleObject(pjogo->hMutex, INFINITE);

			for (int i = 0; i < pjogo->numJogadores; i++)
			{
				if (_tcscmp(pjogo->jogadores[i].nomeUser, nome) == 0)
				{
					TCHAR mensagemSaida[] = _T("sair");
					DWORD bytesWritten;
					OVERLAPPED ov;
					ZeroMemory(&ov, sizeof(OVERLAPPED));
					ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
					if (ov.hEvent == NULL) {
						_tprintf(_T("Erro a criar o evento\n"));
						ReleaseMutex(pjogo->hMutex);
						return 1;
					}
					BOOL ret = WriteFile(pjogo->jogadores[i].hPipe, mensagemSaida, (DWORD)_tcslen(mensagemSaida) * sizeof(TCHAR), &bytesWritten, &ov);
					if (!ret && GetLastError() != ERROR_IO_PENDING) {
						_tprintf(_T("[ERRO] %d (%d bytes)... (WriteFile)\n"), ret, bytesWritten);
						CloseHandle(ov.hEvent);
						ReleaseMutex(pjogo->hMutex);
						return 1;
					}
					if (GetLastError() == ERROR_IO_PENDING) {
						WaitForSingleObject(ov.hEvent, INFINITE); //esperar pelo fim da operaçao
						GetOverlappedResult(pjogo->jogadores[i].hPipe, &ov, &bytesWritten, FALSE); //obter o resultado da operaçao
					}
					CloseHandle(ov.hEvent);
					pjogo->numJogadores--;
					for (int j = i; j < pjogo->numJogadores; j++)
					{
						pjogo->jogadores[j] = pjogo->jogadores[j + 1];
					}
					break;
				}
			}
			ReleaseMutex(pjogo->hMutex);
		}
		else if (_tcscmp(token, _T("iniciarbot")) == 0)
		{
			token = _tcstok_s(NULL, _T(" "), &context);
			if (token == NULL)
			{
				_tprintf(_T("Falta de argumentos\nUtilize 'iniciarbot <username>'\n"));
				continue;
			}

			TCHAR username[USERTAM];
			_tcscpy_s(username, USERTAM, token);

			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL) {
				_tprintf(_T("Demasiados argumentos\nUtilize 'iniciarbot <username>'\n"));
				continue;
			}
			srand(time(NULL));
			int tReacao = (rand() % 26) + 5;


			TCHAR cmd[256];
			_stprintf_s(cmd, 256, _T("bot.exe %s %d"), username, tReacao);

			STARTUPINFO si;
			PROCESS_INFORMATION pi;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));

			if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi))
			{
				_tprintf_s(_T("[ERRO] Iniciar o Bot (%d)\n"), GetLastError());
			}
			else
			{
				_tprintf(_T("Bot '%s' iniciado com tempo de reação %d segundos\n"), username, tReacao);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
		}
		else if (_tcscmp(token, _T("acelerar")) == 0) {
			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL)
			{
				_tprintf(_T("Demasiados argumentos\nUtilize 'acelerar'\n"));
				continue;
			}
			WaitForSingleObject(pjogo->hMutex, INFINITE);
			if (pjogo->ritmo > 1)
			{
				pjogo->ritmo--;
				_tprintf(_T("Ritmo aumentado para %d\n"), pjogo->ritmo);
				TCHAR msg[100];
				_stprintf_s(msg, 100, _T("[Ritmo] Novo ritmo: %d"), pjogo->ritmo);

				for (int i = 0; i < MAX_JOGADORES; i++)
				{
					if (pjogo->jogadores[i].hPipe != NULL)
					{
						OVERLAPPED ovNotificacao;
						ZeroMemory(&ovNotificacao, sizeof(OVERLAPPED));
						ovNotificacao.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
						if (ovNotificacao.hEvent == NULL) {
							_tprintf(_T("Erro a criar o evento de notificação\n"));
							ReleaseMutex(pjogo->hMutex);
							continue;
						}

						DWORD bytesWritten;
						BOOL ret = WriteFile(pjogo->jogadores[i].hPipe, msg, (DWORD)_tcslen(msg) * sizeof(TCHAR), &bytesWritten, &ovNotificacao);
						if (!ret && GetLastError() != ERROR_IO_PENDING) {
							_tprintf(_T("Erro a enviar notificação para %s\n"), pjogo->jogadores[i].nomeUser);
						}
						else if (GetLastError() == ERROR_IO_PENDING) {
							WaitForSingleObject(ovNotificacao.hEvent, INFINITE);
							GetOverlappedResult(pjogo->jogadores[i].hPipe, &ovNotificacao, &bytesWritten, FALSE);
						}
						CloseHandle(ovNotificacao.hEvent);
					}
				}

			}
			else
			{
				_tprintf(_T("Ritmo já no máximo\n"));
			}
			ReleaseMutex(pjogo->hMutex);
		}
		else if (_tcscmp(token, _T("travar")) == 0) {
			token = _tcstok_s(NULL, _T(" "), &context);
			if (token != NULL)
			{
				_tprintf(_T("Demasiados argumentos\nUtilize 'travar'\n"));
				continue;
			}
			WaitForSingleObject(pjogo->hMutex, INFINITE);
			pjogo->ritmo++;
			_tprintf(_T("Ritmo diminuido para %d\n"), pjogo->ritmo);
			TCHAR msg[100];
			_stprintf_s(msg, 100, _T("[Ritmo] Novo ritmo: %d"), pjogo->ritmo);

			for (int i = 0; i < MAX_JOGADORES; i++)
			{
				if (pjogo->jogadores[i].hPipe != NULL)
				{
					OVERLAPPED ovNotificacao;
					ZeroMemory(&ovNotificacao, sizeof(OVERLAPPED));
					ovNotificacao.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
					if (ovNotificacao.hEvent == NULL) {
						_tprintf(_T("Erro a criar o evento de notificação\n"));
						ReleaseMutex(pjogo->hMutex);
						continue;
					}
					DWORD bytesWritten;
					BOOL ret = WriteFile(pjogo->jogadores[i].hPipe, msg, (DWORD)_tcslen(msg) * sizeof(TCHAR), &bytesWritten, &ovNotificacao);
					if (!ret && GetLastError() != ERROR_IO_PENDING) {
						_tprintf(_T("Erro a enviar notificação para %s\n"), pjogo->jogadores[i].nomeUser);
					}
					else if (GetLastError() == ERROR_IO_PENDING) {
						WaitForSingleObject(ovNotificacao.hEvent, INFINITE);
						GetOverlappedResult(pjogo->jogadores[i].hPipe, &ovNotificacao, &bytesWritten, FALSE);
					}
					CloseHandle(ovNotificacao.hEvent);
				}
			}
			ReleaseMutex(pjogo->hMutex);
		}
		else
		{
			_tprintf(_T("Comando desconhecido\n"));
		}
	}
	ExitThread(0);
}

DWORD WINAPI trataJogador(LPVOID data) {
	JOGO* pjogo = (JOGO*)data;
	TCHAR buf[256];
	DWORD bytesRead;
	int indexJogador;

	JOGADOR* jogadorAtual = NULL;

	BOOL ret;
	OVERLAPPED ov;
	HANDLE hEv = CreateEvent(NULL, TRUE, FALSE, NULL);

	WaitForSingleObject(pjogo->hMutex, INFINITE);
	indexJogador = pjogo->jogadorAtual;
	ReleaseMutex(pjogo->hMutex);

	ZeroMemory(&ov, sizeof(OVERLAPPED));
	ov.hEvent = hEv;
	ret = ReadFile(pjogo->jogadores[indexJogador].hPipe, buf, sizeof(buf), &bytesRead, &ov);
	if (!ret && GetLastError() != ERROR_IO_PENDING) {
		_tprintf_s(_T("[ERRO] %d (%d bytes)... (ReadFile)\n"), ret, bytesRead);
		CloseHandle(pjogo->jogadores[indexJogador].hPipe);
		CloseHandle(hEv);
		ExitThread(1);
	}
	if (GetLastError() == ERROR_IO_PENDING) {
		WaitForSingleObject(hEv, INFINITE); //esperar pelo fim da operaçao
		GetOverlappedResult(pjogo->jogadores[indexJogador].hPipe, &ov, &bytesRead, FALSE); //obter o resultado da operaçao
	}

	buf[bytesRead / sizeof(TCHAR)] = '\0';

	for (int i = 0; i + 1 < pjogo->numJogadores; i++) {
		if (_tcscmp(pjogo->jogadores[i].nomeUser, buf) == 0) {
			_tprintf(_T("Já existe um jogador com o nome %s\n"), buf);

			ZeroMemory(&ov, sizeof(OVERLAPPED));
			ov.hEvent = hEv;
			ret = WriteFile(pjogo->jogadores[indexJogador].hPipe, _T("existe"), (DWORD)_tcslen(_T("existe")) * sizeof(TCHAR), &bytesRead, &ov);
			if (!ret && GetLastError() != ERROR_IO_PENDING) {
				_tprintf_s(_T("[ERRO] %d (%d bytes)... (WriteFile)\n"), ret, bytesRead);
			}
			if (GetLastError() == ERROR_IO_PENDING) {
				WaitForSingleObject(hEv, INFINITE);
				GetOverlappedResult(pjogo->jogadores[indexJogador].hPipe, &ov, &bytesRead, FALSE);
			}
			_tprintf(_T("Jogador %s desconectado\n"), pjogo->jogadores[indexJogador].nomeUser);

			WaitForSingleObject(pjogo->hMutex, INFINITE);
			pjogo->jogadores[indexJogador].hPipe = NULL;
			pjogo->jogadores[indexJogador].pontos = 0.0f;
			pjogo->numJogadores--;
			ReleaseMutex(pjogo->hMutex);

			CloseHandle(pjogo->jogadores[indexJogador].hPipe);
			CloseHandle(hEv);
			ExitThread(0);
		}
	}

	WaitForSingleObject(pjogo->hMutex, INFINITE);
	_tcscpy_s(pjogo->jogadores[indexJogador].nomeUser, USERTAM, buf);
	jogadorAtual = &pjogo->jogadores[indexJogador];
	ReleaseMutex(pjogo->hMutex);


	while (pjogo->continua) {
		ZeroMemory(&ov, sizeof(OVERLAPPED));
		ov.hEvent = hEv;
		ret = ReadFile(jogadorAtual->hPipe, buf, sizeof(buf), &bytesRead, &ov);
		if (!ret && GetLastError() != ERROR_IO_PENDING) {
			_tprintf_s(_T("[ERRO] %d (%d bytes)... (ReadFile)\n"), ret, bytesRead);
			break;
		}
		if (GetLastError() == ERROR_IO_PENDING) {
			WaitForSingleObject(hEv, INFINITE); //esperar pelo fim da operaçao
			GetOverlappedResult(jogadorAtual->hPipe, &ov, &bytesRead, FALSE); //obter o resultado da operaçao
		}

		if (bytesRead == 0) {
			_tprintf(_T("Jogador %s desconectado\n"), jogadorAtual->nomeUser);
			break;
		}

		buf[bytesRead / sizeof(TCHAR)] = _T('\0');


		TCHAR resposta[1024];
		TCHAR linha[128];

		if (_tcscmp(buf, _T(":sair")) == 0) {
			_tprintf(_T("Jogador %s desconectado\n"), jogadorAtual->nomeUser);
			break;
		}
		else if (_tcscmp(buf, _T(":jogs")) == 0)
		{
			_tcscpy_s(resposta, 1024, _T("Jogadores:\n"));

			WaitForSingleObject(pjogo->hMutex, INFINITE);
			for (int i = 0; i < pjogo->numJogadores; i++)
			{
				_stprintf_s(linha, 128, _T("%s - %0.1f pontos\n"), pjogo->jogadores[i].nomeUser, pjogo->jogadores[i].pontos);
				_tcscat_s(resposta, 1024, linha);
			}
			ReleaseMutex(pjogo->hMutex);
		}
		else if (_tcscmp(buf, _T(":pont")) == 0) {
			_tcscpy_s(resposta, 1024, _T("Pontuação:"));

			WaitForSingleObject(pjogo->hMutex, INFINITE);
			_stprintf_s(linha, 128, _T(" %.1f pontos\n"), jogadorAtual->pontos);
			_tcscat_s(resposta, 1024, linha);
			ReleaseMutex(pjogo->hMutex);
		}
		else {
			if (pjogo->gameStarted) {
				float wordLen = (float)_tcslen(buf);
				BOOL noDicionario = FALSE;
				BOOL todasLetras = FALSE;

				for (int i = 0; i < pjogo->numPalavras; i++) {
					if (_tcscmp(pjogo->palavras[i], buf) == 0) {
						noDicionario = TRUE;
						break;
					}
				}

				if (noDicionario) {
					todasLetras = TRUE;

					for (int i = 0; i < wordLen; i++) {
						TCHAR charDaPalavra = _totupper(buf[i]);
						BOOL letraValida = FALSE;

						for (int j = 0; j < pjogo->maxLetras; j++) {
							if (pjogo->pData->letras[j] == charDaPalavra) {
								letraValida = TRUE;
								break;
							}
						}

						if (!letraValida) {
							todasLetras = FALSE;
							break;
						}
					}
				}

				if (noDicionario && todasLetras) {
					WaitForSingleObject(pjogo->hMutex, INFINITE);
					jogadorAtual->pontos += wordLen;
					ReleaseMutex(pjogo->hMutex);

					_stprintf_s(resposta, 1024, _T("Palavra '%s' correta! Pontos: %0.1f"), buf, jogadorAtual->pontos);
				}
				else {
					WaitForSingleObject(pjogo->hMutex, INFINITE);
					jogadorAtual->pontos -= wordLen / 2.0f;
					ReleaseMutex(pjogo->hMutex);

					if (!noDicionario) {
						_stprintf_s(resposta, 1024, _T("Palavra '%s' errada! Pontos: %0.1f"), buf, jogadorAtual->pontos);
					}
					else {
						_stprintf_s(resposta, 1024, _T("Letras não disponíveis para a palavra '%s'! Pontos: %0.1f"), buf, jogadorAtual->pontos);
					}
				}
			}
			else {
				_stprintf_s(resposta, 1024, _T("À espera de jogadores para iniciar o jogo!"));
			}
		}

		ZeroMemory(&ov, sizeof(OVERLAPPED));
		ov.hEvent = hEv;
		DWORD bytesWritten;

		ret = WriteFile(jogadorAtual->hPipe,
			resposta,
			(DWORD)_tcslen(resposta) * sizeof(TCHAR),
			&bytesWritten,
			&ov);

		if (!ret && GetLastError() == ERROR_IO_PENDING) {
			WaitForSingleObject(hEv, INFINITE);
			GetOverlappedResult(jogadorAtual->hPipe, &ov, &bytesWritten, FALSE);
		}
	}


	ZeroMemory(&ov, sizeof(OVERLAPPED));
	ov.hEvent = hEv;
	ret = WriteFile(jogadorAtual->hPipe, _T("sair"), (DWORD)_tcslen(_T("sair")) * sizeof(TCHAR), &bytesRead, &ov);
	if (!ret && GetLastError() != ERROR_IO_PENDING) {
		_tprintf_s(_T("[ERRO] %d (%d bytes)... (WriteFile)\n"), ret, bytesRead);
	}
	if (GetLastError() == ERROR_IO_PENDING) {
		WaitForSingleObject(hEv, INFINITE); //esperar pelo fim da operaçao
		GetOverlappedResult(jogadorAtual->hPipe, &ov, &bytesRead, FALSE); //obter o resultado da operaçao
	}
	_tprintf(_T("Jogador %s desconectado\n"), jogadorAtual->nomeUser);

	WaitForSingleObject(pjogo->hMutex, INFINITE);
	for (int i = 0; i < pjogo->numJogadores; i++)
	{
		if (_tcscmp(pjogo->jogadores[i].nomeUser, jogadorAtual->nomeUser) == 0)
		{
			indexJogador = i;
			break;
		}
	}
	pjogo->jogadores[indexJogador].nomeUser[0] = '\0';
	CloseHandle(pjogo->jogadores[indexJogador].hPipe);
	pjogo->jogadores[indexJogador].hPipe = NULL;
	pjogo->jogadores[indexJogador].pontos = 0.0f;
	pjogo->numJogadores--;

	JOGADOR* tempJogadores = (JOGADOR*)malloc(pjogo->numJogadores * sizeof(JOGADOR));

	tempJogadores = pjogo->jogadores;

	for (int i = indexJogador; i < pjogo->numJogadores; i++)
	{
		pjogo->jogadores[i] = tempJogadores[i + 1];
	}

	free(tempJogadores);

	ReleaseMutex(pjogo->hMutex);

	CloseHandle(hEv);
	ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[]) {
	HANDLE hLockMutex;
	LSTATUS res;
	HKEY hKey;
	DWORD MAXLETRAS, RITMO, estado;
	DWORD size = sizeof(DWORD);
	HANDLE hPipe;
	TCHAR buf[256];
	int index = 0;
	JOGO jogo;
	HANDLE hThreadL = NULL, hThreadT;


#ifdef UNICODE
	(void) _setmode(_fileno(stdin), _O_WTEXT);
	(void)_setmode(_fileno(stdout), _O_WTEXT);
	(void)_setmode(_fileno(stderr), _O_WTEXT);
#endif

	hLockMutex = CreateMutex(NULL, FALSE, NOME_MUTEX_LOCK);

	if (hLockMutex == NULL) {
		_tprintf(_T("Erro a criar o mutex lock\n"));
		return 1;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		_tprintf(_T("Já existe uma instância do árbitro em execução.\n"));
		CloseHandle(hLockMutex);
		return 1;
	}

	res = RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\TrabSO2"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &estado);
	if (res != ERROR_SUCCESS)
	{
		_tprintf(_T("Erro a criar a chave\n"));
		return 1;
	}
	res = RegQueryValueEx(hKey, _T("MAXLETRAS"), NULL, NULL, (LPBYTE)&MAXLETRAS, &size);
	if (res != ERROR_SUCCESS)
	{
		_tprintf(_T("Erro a ler o valor MAXLETRAS\n"));
		MAXLETRAS = 6;
	}
	if (MAXLETRAS > 12)
	{
		MAXLETRAS = 12;
	}

	res = RegQueryValueEx(hKey, _T("RITMO"), NULL, NULL, (LPBYTE)&RITMO, &size);
	if (res != ERROR_SUCCESS)
	{
		_tprintf(_T("Erro a ler o valor RITMO\n"));
		RITMO = 3;
	}

	_tprintf(_T("MAXLETRAS: %d\n"), MAXLETRAS);
	_tprintf(_T("RITMO: %d\n"), RITMO);

	RegCloseKey(hKey);

	CarregarPalavras(&jogo);

	srand((unsigned)time(NULL));

	jogo.ritmo = RITMO;
	jogo.maxLetras = MAXLETRAS;
	jogo.continua = TRUE;
	jogo.numJogadores = 0;
	jogo.gameStarted = FALSE;

	jogo.jogadores = (JOGADOR*)malloc(MAX_JOGADORES * sizeof(JOGADOR));
	if (jogo.jogadores == NULL) {
		_tprintf(_T("Erro a alocar memória para os jogadores\n"));
		CloseHandle(hLockMutex);
		return 1;
	}
	for (int i = 0; i < MAX_JOGADORES; i++) {
		jogo.jogadores[i].hPipe = NULL;
		jogo.jogadores[i].pontos = 0.0f;
		jogo.jogadores->nomeUser[0] = _T('\0');
	}


	jogo.hEventStop = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (jogo.hEventStop == NULL) {
		_tprintf(_T("Erro a criar o evento\n"));
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}


	jogo.hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, TAM * sizeof(TCHAR), NOME_MAPA);
	if (jogo.hMap == NULL)
	{
		_tprintf(_T("Erro a criar o mapeamento de ficheiro\n"));
		CloseHandle(jogo.hEventStop);
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}

	jogo.pData = (SDATA*)MapViewOfFile(jogo.hMap, FILE_MAP_ALL_ACCESS, 0, 0, TAM * sizeof(TCHAR));
	if (jogo.pData == NULL)
	{
		_tprintf(_T("Erro a mapear o ficheiro\n"));
		CloseHandle(jogo.hEventStop);
		CloseHandle(jogo.hMap);
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}
	jogo.hMutex = CreateMutex(NULL, FALSE, NOME_MUTEX);
	if (jogo.hMutex == NULL)
	{
		_tprintf(_T("Erro a criar o mutex\n"));
		CloseHandle(jogo.hEventStop);
		UnmapViewOfFile(jogo.pData);
		CloseHandle(jogo.hMap);
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}
	jogo.hEvent = CreateEvent(NULL, TRUE, FALSE, NOME_EVENTO);
	if (jogo.hEvent == NULL)
	{
		_tprintf(_T("Erro a criar o evento\n"));
		CloseHandle(jogo.hEventStop);
		CloseHandle(jogo.hMutex);
		UnmapViewOfFile(jogo.pData);
		CloseHandle(jogo.hMap);
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}

	for (int i = 0; i < TAM; i++)
	{
		if (i < MAXLETRAS)
		{
			jogo.pData->letras[i] = '\0';
		}
		else
		{
			jogo.pData->letras[i] = _T('_');
		}
	}

	hThreadT = CreateThread(NULL, 0, leTeclado, (LPVOID)&jogo, 0, NULL);
	if (hThreadT == NULL)
	{
		_tprintf(_T("Erro a criar a thread do teclado\n"));
		CloseHandle(jogo.hEventStop);
		CloseHandle(jogo.hEvent);
		CloseHandle(jogo.hMutex);
		UnmapViewOfFile(jogo.pData);
		CloseHandle(jogo.hMap);
		free(jogo.jogadores);
		CloseHandle(hLockMutex);
		return 1;
	}

	while (jogo.continua) {
		if (!jogo.gameStarted && jogo.numJogadores == 2) {
			hThreadL = CreateThread(NULL, 0, ThreadLetras, (LPVOID)&jogo, 0, NULL);
			if (hThreadL == NULL)
			{
				_tprintf(_T("Erro a criar a thread\n"));
				CloseHandle(jogo.hEventStop);
				CloseHandle(jogo.hEvent);
				CloseHandle(jogo.hMutex);
				UnmapViewOfFile(jogo.pData);
				CloseHandle(jogo.hMap);
				free(jogo.jogadores);
				CloseHandle(hLockMutex);
				return 1;
			}
			jogo.gameStarted = TRUE;
		}
		hPipe = CreateNamedPipe(NOME_PIPE, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_JOGADORES, sizeof(buf), sizeof(buf),
			NMPWAIT_USE_DEFAULT_WAIT,
			NULL);

		OVERLAPPED ov;
		ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (ov.hEvent == NULL) {
			_tprintf(_T("Erro a criar o evento para OVERLAPPED: %lu\n"), GetLastError());
			CloseHandle(hPipe);
			if (!jogo.continua) break;
			continue;
		}

		BOOL connected = ConnectNamedPipe(hPipe, &ov);
		DWORD dwError = GetLastError();
		BOOL playerConnected = FALSE;

		if (connected) {
			playerConnected = TRUE;
		}
		else if (dwError == ERROR_IO_PENDING) {
			_tprintf(_T("ConnectNamedPipe pendente.\n"));
			HANDLE waitHandles[2] = { ov.hEvent, jogo.hEventStop };
			DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

			if (waitResult == WAIT_OBJECT_0) {
				DWORD bytesTransferred;
				if (GetOverlappedResult(hPipe, &ov, &bytesTransferred, FALSE)) {
					playerConnected = TRUE;
				}

			}
			else if (waitResult == WAIT_OBJECT_0 + 1) {
				CloseHandle(hPipe);

			}
			else {
				CloseHandle(hPipe);
			}
		}


		CloseHandle(ov.hEvent);

		if (!jogo.continua) {
			if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);
			break;
		}

		if (!playerConnected) {
			if (hPipe != INVALID_HANDLE_VALUE)
				CloseHandle(hPipe);
			_tprintf(_T("Conexão falhou.\n"));
			continue;
		}

		WaitForSingleObject(jogo.hMutex, INFINITE);
		for (int i = 0; i < MAX_JOGADORES; i++) {
			if (jogo.jogadores[i].hPipe == NULL) {
				jogo.jogadorAtual = i;
				jogo.jogadores[i].hPipe = hPipe;
				jogo.jogadores[i].pontos = 0.0f;
				jogo.numJogadores++;
				break;
			}
		}
		ReleaseMutex(jogo.hMutex);
		HANDLE hThread = CreateThread(NULL, 0, trataJogador, (LPVOID)&jogo, 0, NULL);
	}

	WaitForSingleObject(hThreadT, INFINITE);
	CloseHandle(hThreadT);
	if (jogo.gameStarted) {
		WaitForSingleObject(hThreadL, INFINITE);
		CloseHandle(hThreadL);
	}

	CloseHandle(jogo.hEventStop);
	UnmapViewOfFile(jogo.pData);
	CloseHandle(jogo.hMap);
	CloseHandle(jogo.hEvent);
	CloseHandle(jogo.hMutex);
	free(jogo.jogadores);
	CloseHandle(hLockMutex);


	return 0;
}