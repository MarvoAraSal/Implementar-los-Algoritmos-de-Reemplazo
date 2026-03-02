


#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <windows.h>
#include <psapi.h>
#include <iomanip>
#include <random>

#pragma comment(lib, "psapi.lib")

using namespace std;

////////////////////////////////////////////////////////////
// INTERRUPCIONES REALES DEL SISTEMA
////////////////////////////////////////////////////////////

typedef struct _SYSTEM_PERFORMANCE_INFORMATION {
    BYTE Reserved1[312];
    ULONGLONG InterruptCount;
} SYSTEM_PERFORMANCE_INFORMATION;

typedef NTSTATUS(NTAPI* NtQuerySystemInformation_t)(
    int, PVOID, ULONG, PULONG);

#define SystemPerformanceInformation 2

ULONGLONG obtenerInterrupcionesSistema() {

    HMODULE hNtDll = GetModuleHandle(L"ntdll.dll");
    if (!hNtDll) return 0;

    auto NtQuerySystemInformation =
        (NtQuerySystemInformation_t)GetProcAddress(
            hNtDll, "NtQuerySystemInformation");

    if (!NtQuerySystemInformation) return 0;

    SYSTEM_PERFORMANCE_INFORMATION spi = {};
    ULONG retLen = 0;

    NTSTATUS status = NtQuerySystemInformation(
        SystemPerformanceInformation,
        &spi,
        sizeof(spi),
        &retLen
    );

    if (status != 0) return 0;

    return spi.InterruptCount;
}

////////////////////////////////////////////////////////////
// ESTRUCTURAS
////////////////////////////////////////////////////////////

struct Resultado {
    int fallos;
    int hits;
    vector<vector<int>> tabla;
    vector<char> estado;
};

struct Metricas {
    SIZE_T memoriaKB;
    double tiempoMs;
    ULONGLONG syscalls;
    ULONGLONG interrupciones;
    double tiempoKernelMs;
};

////////////////////////////////////////////////////////////
// MÉTRICAS
////////////////////////////////////////////////////////////

Metricas obtenerMetricas(double tiempoMs,
    ULONGLONG intAntes,
    ULONGLONG intDespues) {

    PROCESS_MEMORY_COUNTERS pmc = {};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

    IO_COUNTERS io = {};
    GetProcessIoCounters(GetCurrentProcess(), &io);

    FILETIME create, exit, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &create, &exit, &kernel, &user);

    ULARGE_INTEGER k;
    k.LowPart = kernel.dwLowDateTime;
    k.HighPart = kernel.dwHighDateTime;

    Metricas m;
    m.memoriaKB = pmc.WorkingSetSize / 1024;
    m.tiempoMs = tiempoMs;
    m.syscalls = io.ReadOperationCount +
        io.WriteOperationCount +
        io.OtherOperationCount;
    m.interrupciones = (intDespues > intAntes) ?
        (intDespues - intAntes) : 0;
    m.tiempoKernelMs = k.QuadPart / 10000.0;

    return m;
}

void mostrarMetricas(const Metricas& m) {

    cout << "\n=========== METRICAS ===========\n";
    cout << "Memoria usada: " << m.memoriaKB << " KB\n";
    cout << "Tiempo ejecucion: " << m.tiempoMs << " ms\n";
    cout << "Syscalls (IO reales): " << m.syscalls << "\n";
    cout << "Interrupciones HW: " << m.interrupciones << "\n";
    cout << "Tiempo modo Kernel: " << m.tiempoKernelMs << " ms\n";
}

////////////////////////////////////////////////////////////
// GENERAR CADENA
////////////////////////////////////////////////////////////

vector<int> generarCadena(int n) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 9);

    vector<int> paginas(n);
    for (int i = 0; i < n; i++)
        paginas[i] = dis(gen);

    return paginas;
}

////////////////////////////////////////////////////////////
// ALGORITMOS
////////////////////////////////////////////////////////////

Resultado FIFO(vector<int> paginas, int m);
Resultado OPTIMO(vector<int> paginas, int m);
Resultado LRU(vector<int> paginas, int m);
Resultado LFU(vector<int> paginas, int m);
Resultado NFU(vector<int> paginas, int m);
Resultado CLOCK(vector<int> paginas, int m);
Resultado SEGUNDA(vector<int> paginas, int m);
Resultado NRU(vector<int> paginas, int m);

////////////////////////////////////////////////////////////
// IMPLEMENTACIONES
////////////////////////////////////////////////////////////

Resultado FIFO(vector<int> paginas, int m) {

    vector<int> frames(m, -1);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int pointer = 0, fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {
        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it == frames.end()) {
            auto empty = find(frames.begin(), frames.end(), -1);
            if (empty != frames.end())
                *empty = p;
            else {
                frames[pointer] = p;
                pointer = (pointer + 1) % m;
            }
            fallos++; estado.push_back('F');
        }
        else {
            hits++; estado.push_back('H');
        }

        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }

    return { fallos, hits, tabla, estado };
}

Resultado OPTIMO(vector<int> paginas, int m) {

    vector<int> frames(m, -1);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {

        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            hits++; estado.push_back('H');
        }
        else {

            int idx = -1;

            for (int i = 0; i < m; i++)
                if (frames[i] == -1) { idx = i; break; }

            if (idx == -1) {
                int farthest = -1;
                for (int i = 0; i < m; i++) {
                    int j;
                    for (j = t + 1; j < paginas.size(); j++)
                        if (frames[i] == paginas[j]) break;
                    if (j > farthest) {
                        farthest = j;
                        idx = i;
                    }
                }
            }

            frames[idx] = p;
            fallos++; estado.push_back('F');
        }

        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }

    return { fallos, hits, tabla, estado };
}

Resultado LRU(vector<int> paginas, int m) {

    vector<int> frames(m, -1);
    vector<int> last(m, 0);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int time = 0, fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {
        time++;
        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            int idx = distance(frames.begin(), it);
            last[idx] = time;
            hits++; estado.push_back('H');
        }
        else {
            int idx = -1;
            for (int i = 0; i < m; i++)
                if (frames[i] == -1) { idx = i; break; }
            if (idx == -1)
                idx = min_element(last.begin(), last.end()) - last.begin();
            frames[idx] = p;
            last[idx] = time;
            fallos++; estado.push_back('F');
        }
        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }

    return { fallos, hits, tabla, estado };
}

Resultado LFU(vector<int> paginas, int m) {
    vector<int> frames(m, -1), freq(m, 0);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {
        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            int idx = distance(frames.begin(), it);
            freq[idx]++;
            hits++; estado.push_back('H');
        }
        else {
            int idx = -1;
            for (int i = 0; i < m; i++)
                if (frames[i] == -1) { idx = i; break; }
            if (idx == -1)
                idx = min_element(freq.begin(), freq.end()) - freq.begin();
            frames[idx] = p;
            freq[idx] = 1;
            fallos++; estado.push_back('F');
        }
        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }
    return { fallos,hits,tabla,estado };
}

Resultado NFU(vector<int> paginas, int m) {
    return LFU(paginas, m); // simplificado
}

Resultado CLOCK(vector<int> paginas, int m) {
    vector<int> frames(m, -1);
    vector<bool> ref(m, false);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int pointer = 0, fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {
        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            ref[distance(frames.begin(), it)] = true;
            hits++; estado.push_back('H');
        }
        else {
            while (frames[pointer] != -1 && ref[pointer]) {
                ref[pointer] = false;
                pointer = (pointer + 1) % m;
            }
            frames[pointer] = p;
            ref[pointer] = true;
            pointer = (pointer + 1) % m;
            fallos++; estado.push_back('F');
        }
        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }
    return { fallos,hits,tabla,estado };
}

Resultado SEGUNDA(vector<int> paginas, int m) {
    return CLOCK(paginas, m);
}

Resultado NRU(vector<int> paginas, int m) {
    vector<int> frames(m, -1);
    vector<bool> ref(m, false);
    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;
    int fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {
        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            ref[distance(frames.begin(), it)] = true;
            hits++; estado.push_back('H');
        }
        else {
            int idx = -1;
            for (int i = 0; i < m; i++)
                if (frames[i] == -1 || !ref[i]) { idx = i; break; }
            if (idx == -1) {
                fill(ref.begin(), ref.end(), false);
                idx = 0;
            }
            frames[idx] = p;
            ref[idx] = true;
            fallos++; estado.push_back('F');
        }
        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }
    return { fallos,hits,tabla,estado };
}

////////////////////////////////////////////////////////////
// MAIN
////////////////////////////////////////////////////////////

int main() {

    int n, marcos, op;

    cout << "Cantidad referencias: ";
    cin >> n;
    cout << "Numero marcos: ";
    cin >> marcos;

    vector<int> paginas = generarCadena(n);

    cout << "\n1 FIFO\n2 OPTIMO\n3 LRU\n4 LFU\n5 NFU\n6 CLOCK\n7 SEGUNDA\n8 NRU\n9 TODOS\n";
    cin >> op;

    auto ejecutar = [&](string nombre, auto algoritmo) {

        ULONGLONG intAntes = obtenerInterrupcionesSistema();
        auto inicio = chrono::high_resolution_clock::now();
        Resultado r = algoritmo(paginas, marcos);
        auto fin = chrono::high_resolution_clock::now();
        ULONGLONG intDespues = obtenerInterrupcionesSistema();

        double tiempo =
            chrono::duration<double, milli>(fin - inicio).count();

        Metricas m = obtenerMetricas(tiempo, intAntes, intDespues);

        cout << "\n====== " << nombre << " ======\n";
        cout << "Fallos: " << r.fallos << " Hits: " << r.hits << "\n";
        mostrarMetricas(m);
        };

    switch (op) {
    case 1: ejecutar("FIFO", FIFO); break;
    case 2: ejecutar("OPTIMO", OPTIMO); break;
    case 3: ejecutar("LRU", LRU); break;
    case 4: ejecutar("LFU", LFU); break;
    case 5: ejecutar("NFU", NFU); break;
    case 6: ejecutar("CLOCK", CLOCK); break;
    case 7: ejecutar("SEGUNDA", SEGUNDA); break;
    case 8: ejecutar("NRU", NRU); break;
    case 9:
        ejecutar("FIFO", FIFO);
        ejecutar("OPTIMO", OPTIMO);
        ejecutar("LRU", LRU);
        ejecutar("LFU", LFU);
        ejecutar("NFU", NFU);
        ejecutar("CLOCK", CLOCK);
        ejecutar("SEGUNDA", SEGUNDA);
        ejecutar("NRU", NRU);
        break;
    }

    return 0;
}


