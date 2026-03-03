// MARCO ANTONIO ARAPA SALAZAR
// Codigo de alumno ==> 225417

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <windows.h>
#include <psapi.h>
#include <iomanip>
#include <random>
#include <fstream>
#include <string>
#include <thread>
#include <conio.h>

#pragma comment(lib, "psapi.lib")

using namespace std;

void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void limpiarPantalla() {
    system("cls");
}

#define COLOR_DEFAULT 7
#define COLOR_RED 12
#define COLOR_GREEN 10
#define COLOR_GRAY 8

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



void dibujarTablaHorizontal(
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado)
{
    int filas = tabla.size();
    int columnas = referencias.size();

    int ancho = 4; // ancho base
    int anchoCol = max(6, ancho);

    // ====== ENCABEZADO TIEMPO ======
    cout << "\n";
    cout << "Tiempo ";
    for (int t = 0; t < columnas; t++) {
        cout << "|" << setw(anchoCol) << t + 1;
    }
    cout << "|\n";

    // línea separadora
    cout << string(8 + columnas * (anchoCol + 1), '-') << "\n";

    // ====== FILA REFERENCIAS ======
    cout << "Ref    ";
    for (int t = 0; t < columnas; t++) {
        cout << "|" << setw(anchoCol) << referencias[t];
    }
    cout << "|\n";

    cout << string(8 + columnas * (anchoCol + 1), '-') << "\n";

    // ====== MARCOS ======
    for (int i = 0; i < filas; i++) {
        cout << "M" << i + 1 << "     ";
        for (int t = 0; t < columnas; t++) {

            cout << "|";

            if (tabla[i][t] == -1) {
                setColor(COLOR_GRAY);
                cout << setw(anchoCol) << "-";
            }
            else {
                setColor(COLOR_DEFAULT);
                cout << setw(anchoCol) << tabla[i][t];
            }
            setColor(COLOR_DEFAULT);
        }
        cout << "|\n";
    }

    cout << string(8 + columnas * (anchoCol + 1), '-') << "\n";

    // ====== FILA F/H ======
    cout << "Estado ";
    for (int t = 0; t < columnas; t++) {

        cout << "|";

        if (estado[t] == 'F') {
            setColor(COLOR_RED);
        }
        else {
            setColor(COLOR_GREEN);
        }

        cout << setw(anchoCol) << estado[t];
        setColor(COLOR_DEFAULT);
    }
    cout << "|\n";

    cout << string(8 + columnas * (anchoCol + 1), '=') << "\n";
}

void exportarCSV(const string& nombreArchivo,
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado)
{
    ofstream file(nombreArchivo);

    file << "Tiempo,";
    for (int t = 0; t < referencias.size(); t++)
        file << t + 1 << ",";
    file << "\n";

    file << "Referencia,";
    for (int r : referencias)
        file << r << ",";
    file << "\n";

    for (int i = 0; i < tabla.size(); i++) {
        file << "M" << i + 1 << ",";
        for (int t = 0; t < referencias.size(); t++)
            file << tabla[i][t] << ",";
        file << "\n";
    }

    file << "Estado,";
    for (char c : estado)
        file << c << ",";
    file << "\n";

    file.close();
}

void exportarHTML(const string& nombreArchivo,
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado)
{
    ofstream file(nombreArchivo);

    file << "<html><head><style>";
    file << "table {border-collapse: collapse;}";
    file << "td,th {border:1px solid black; padding:5px; text-align:center;}";
    file << ".F {background-color:#ff9999;}";
    file << ".H {background-color:#99ff99;}";
    file << ".empty {background-color:#dddddd;}";
    file << "</style></head><body>";

    file << "<table>";

    // Tiempo
    file << "<tr><th>Tiempo</th>";
    for (int t = 0; t < referencias.size(); t++)
        file << "<th>" << t + 1 << "</th>";
    file << "</tr>";

    // Referencia
    file << "<tr><th>Referencia</th>";
    for (int r : referencias)
        file << "<td>" << r << "</td>";
    file << "</tr>";

    // Marcos
    for (int i = 0; i < tabla.size(); i++) {
        file << "<tr><th>M" << i + 1 << "</th>";
        for (int t = 0; t < referencias.size(); t++) {
            if (tabla[i][t] == -1)
                file << "<td class='empty'>-</td>";
            else
                file << "<td>" << tabla[i][t] << "</td>";
        }
        file << "</tr>";
    }

    // Estado
    file << "<tr><th>Estado</th>";
    for (char c : estado) {
        if (c == 'F')
            file << "<td class='F'>F</td>";
        else
            file << "<td class='H'>H</td>";
    }
    file << "</tr>";

    file << "</table></body></html>";

    file.close();
}

void dibujarTablaAnimada(
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado,
    int pasoActual)
{
    int filas = tabla.size();
    int columnas = referencias.size();
    int ancho = 5;

    limpiarPantalla();

    cout << "\n SIMULADOR DE REEMPLAZO DE PAGINAS\n\n";

    // ===== BORDE SUPERIOR =====
    cout << "?????????";
    for (int t = 0; t <= pasoActual; t++)
        cout << "??????";
    cout << "?\n";

    // ===== FILA TIEMPO =====
    cout << "?Tiempo  ";
    for (int t = 0; t <= pasoActual; t++)
        cout << "?" << setw(ancho) << t + 1;
    cout << "?\n";

    // ===== SEPARADOR =====
    cout << "?????????";
    for (int t = 0; t <= pasoActual; t++)
        cout << "??????";
    cout << "?\n";

    // ===== REFERENCIAS =====
    cout << "?Ref     ";
    for (int t = 0; t <= pasoActual; t++)
        cout << "?" << setw(ancho) << referencias[t];
    cout << "?\n";

    // ===== MARCOS =====
    for (int i = 0; i < filas; i++) {
        cout << "?M" << i + 1 << "      ";
        for (int t = 0; t <= pasoActual; t++) {

            cout << "?";

            if (tabla[i][t] == -1) {
                setColor(COLOR_GRAY);
                cout << setw(ancho) << "-";
            }
            else {
                setColor(COLOR_DEFAULT);
                cout << setw(ancho) << tabla[i][t];
            }
            setColor(COLOR_DEFAULT);
        }
        cout << "?\n";
    }

    // ===== ESTADO =====
    cout << "?Estado  ";
    for (int t = 0; t <= pasoActual; t++) {

        cout << "?";

        if (estado[t] == 'F')
            setColor(COLOR_RED);
        else
            setColor(COLOR_GREEN);

        cout << setw(ancho) << estado[t];
        setColor(COLOR_DEFAULT);
    }
    cout << "?\n";

    // ===== BORDE INFERIOR =====
    cout << "?????????";
    for (int t = 0; t <= pasoActual; t++)
        cout << "??????";
    cout << "?\n";
}

void animarSimulacion(
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado)
{
    for (int paso = 0; paso < referencias.size(); paso++) {

        dibujarTablaAnimada(referencias, tabla, estado, paso);

        cout << "\nPresiona cualquier tecla para siguiente paso...\n";
        _getch();
    }

    cout << "\nSimulacion completa.\n";
}

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

        animarSimulacion(paginas, r.tabla, r.estado);

        auto fin = chrono::high_resolution_clock::now();
        ULONGLONG intDespues = obtenerInterrupcionesSistema();

        double tiempo =
            chrono::duration<double, milli>(fin - inicio).count();

        Metricas m = obtenerMetricas(tiempo, intAntes, intDespues);

        cout << "\n====== " << nombre << " ======\n";
        cout << "Fallos: " << r.fallos << " Hits: " << r.hits << "\n";

        dibujarTablaHorizontal(paginas, r.tabla, r.estado);
        exportarCSV(nombre + ".csv", paginas, r.tabla, r.estado);
        exportarHTML(nombre + ".html", paginas, r.tabla, r.estado);

        mostrarMetricas(m);
        };

    bool continuar = true;

    while (continuar) {

        cout << "\n1 FIFO\n2 OPTIMO\n3 LRU\n4 LFU\n5 NFU\n6 CLOCK\n7 SEGUNDA\n8 NRU\n9 TODOS\n";
        cin >> op;

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

        cout << "\n¿Deseas probar otro algoritmo? (1=SI / 0=NO): ";
        cin >> continuar;

        if (continuar) {
            paginas = generarCadena(n);
        }
    }

    return 0;
}

