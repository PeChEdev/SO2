#pragma once
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#define TAM 12
#define USERTAM 32
#define MAX_JOGADORES 20
#define TAM_PALAVRA 12
#define MAX_PALAVRA 10
#define NOME_MAPA _T("MapaPartilhado")
#define NOME_MUTEX _T("mutexPartilhado")
#define NOME_EVENTO _T("eventoPartilhado")
#define NOME_PIPE _T("\\\\.\\pipe\\PipeJogo")
#define NOME_MUTEX_LOCK _T("mutexLockArbitro")

typedef struct {
    char letras[TAM];
}SDATA;


typedef struct {
    HANDLE hPipe;
    TCHAR nomeUser[USERTAM];
    float pontos;
}JOGADOR;

typedef struct {
    SDATA* pData;
    HANDLE hMap;
    HANDLE hMutex;
    HANDLE hEvent, hEventStop;

    JOGADOR* jogadores;
    int numJogadores;
    int jogadorAtual;

    DWORD ritmo;
    DWORD maxLetras;

    BOOL continua;
    BOOL gameStarted;

    //Palavras no Ficheiro TXT
    TCHAR palavras[MAX_PALAVRA][TAM_PALAVRA];
    int numPalavras;
    TCHAR letrasValidas[26];
    int numLetrasValidas;

    //BOT
    DWORD tempoReacao;
} JOGO;

typedef struct {
    SDATA* pData;

    HANDLE hMutex;
    HANDLE hEvent, hEventStop;
    HANDLE hPipe;

    TCHAR nomeUser[USERTAM];

    BOOL continua;
}TDATAJogo;

typedef struct {
    SDATA* pData;
    HANDLE hMutex;
    HANDLE hEvent;
    HANDLE hEventStop;
    HANDLE hPipe;
    TCHAR nomeUser[USERTAM];
    BOOL continua;
    int tempoReacao;
} TBOTDATA;