// Al// MARCO ANTONIO ARAPA SALAZAR
// Codigo de alumno ==> 225417

#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <iomanip>
#include <random>
#include <fstream>
#include <string>
#include <thread>
#include <conio.h>
#include <limits>

#pragma comment(lib, "psapi.lib")

using namespace std;

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

////////////////////////////////////////////////////////////
// CONFIGURACION GLOBAL
////////////////////////////////////////////////////////////

const int RETARDO_ANIMACION_MS = 350;
const int TAMANO_TLB = 4;

void setColor(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

void limpiarPantalla() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    GetConsoleScreenBufferInfo(hConsole, &csbi);
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    FillConsoleOutputCharacter(hConsole, TEXT(' '),
        dwConSize, coordScreen, &cCharsWritten);

    SetConsoleCursorPosition(hConsole, coordScreen);
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

struct Comparacion {
    string nombre;
    int fallos;
    int hits;
    double tiempo;
};

////////////////////////////////////////////////////////////
// MÉTRICAS
////////////////////////////////////////////////////////////

Metricas obtenerMetricas(double tiempoMs,
    ULONGLONG intAntes,
    ULONGLONG intDespues,
    ULONGLONG kernelAntes,
    ULONGLONG kernelDespues) {

    PROCESS_MEMORY_COUNTERS pmc = {};
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));

    IO_COUNTERS io = {};
    GetProcessIoCounters(GetCurrentProcess(), &io);

    Metricas m;
    m.memoriaKB = pmc.WorkingSetSize / 1024;
    m.tiempoMs = tiempoMs;
    m.syscalls = io.ReadOperationCount +
        io.WriteOperationCount +
        io.OtherOperationCount;

    m.interrupciones = (intDespues > intAntes) ?
        (intDespues - intAntes) : 0;

    m.tiempoKernelMs =
        (kernelDespues - kernelAntes) / 10000.0;

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

    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 9);

    vector<int> paginas(n);

    for (int i = 0; i < n; i++)
        paginas[i] = dis(gen);

    return paginas;
}

////////////////////////////////////////////////////////////
// ALGORITMOS
////////////////////////////////////////////////////////////

Resultado FIFO(const vector<int>& paginas, int m);
Resultado OPTIMO(const vector<int>& paginas, int m);
Resultado LRU(const vector<int>& paginas, int m);
Resultado LFU(const vector<int>& paginas, int m);
Resultado NFU(const vector<int>& paginas, int m);
Resultado CLOCK(const vector<int>& paginas, int m);
Resultado SEGUNDA(const vector<int>& paginas, int m);
Resultado NRU(const vector<int>& paginas, int m);

////////////////////////////////////////////////////////////
// IMPLEMENTACIONES
////////////////////////////////////////////////////////////

Resultado FIFO(const vector<int>& paginas, int m) {

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

Resultado OPTIMO(const vector<int>& paginas, int m) {

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

Resultado LRU(const vector<int>& paginas, int m) {

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

Resultado LFU(const vector<int>& paginas, int m) {
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

Resultado NFU(const vector<int>& paginas, int m) {

    vector<int> frames(m, -1);
    vector<int> contador(m, 0);
    vector<bool> ref(m, false);

    vector<vector<int>> tabla(m, vector<int>(paginas.size()));
    vector<char> estado;

    int fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {

        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        // Actualizar contadores (simula desplazamiento)
        for (int i = 0; i < m; i++) {
            contador[i] >>= 1;
            if (ref[i])
                contador[i] |= (1 << 7);  // bit alto
            ref[i] = false;
        }

        if (it != frames.end()) {
            int idx = distance(frames.begin(), it);
            ref[idx] = true;
            hits++;
            estado.push_back('H');
        }
        else {

            int idx = -1;

            for (int i = 0; i < m; i++)
                if (frames[i] == -1) { idx = i; break; }

            if (idx == -1)
                idx = min_element(contador.begin(), contador.end()) - contador.begin();

            frames[idx] = p;
            contador[idx] = 0;
            ref[idx] = true;

            fallos++;
            estado.push_back('F');
        }

        for (int i = 0; i < m; i++)
            tabla[i][t] = frames[i];
    }

    return { fallos, hits, tabla, estado };
}

Resultado CLOCK(const vector<int>& paginas, int m) {
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

Resultado SEGUNDA(const vector<int>& paginas, int m) {

    vector<int> frames;
    vector<bool> ref;
    vector<vector<int>> tabla(m, vector<int>(paginas.size(), -1));
    vector<char> estado;

    int fallos = 0, hits = 0;

    for (int t = 0; t < paginas.size(); t++) {

        int p = paginas[t];
        auto it = find(frames.begin(), frames.end(), p);

        if (it != frames.end()) {
            int idx = distance(frames.begin(), it);
            ref[idx] = true;
            hits++;
            estado.push_back('H');
        }
        else {

            if (frames.size() < m) {
                frames.push_back(p);
                ref.push_back(true);
            }
            else {
                while (true) {
                    if (!ref[0]) {
                        frames.erase(frames.begin());
                        ref.erase(ref.begin());
                        frames.push_back(p);
                        ref.push_back(true);
                        break;
                    }
                    else {
                        ref[0] = false;
                        int temp = frames[0];
                        frames.erase(frames.begin());
                        ref.erase(ref.begin());
                        frames.push_back(temp);
                        ref.push_back(false);
                    }
                }
            }

            fallos++;
            estado.push_back('F');
        }

        for (int i = 0; i < m; i++)
            if (i < frames.size())
                tabla[i][t] = frames[i];
    }

    return { fallos, hits, tabla, estado };
}

Resultado NRU(const vector<int>& paginas, int m) {
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
    limpiarPantalla();

    int filas = tabla.size();
    int ancho = 6;

    cout << "\n=============================================\n";
    cout << "     SIMULADOR DE REEMPLAZO DE PAGINAS\n";
    cout << "=============================================\n\n";

    cout << "Referencia actual: ";
    setColor(COLOR_GREEN);
    cout << referencias[pasoActual] << "\n";
    setColor(COLOR_DEFAULT);

    cout << "Estado: ";
    if (estado[pasoActual] == 'F') {
        setColor(COLOR_RED);
        cout << "PAGE FAULT\n";
    }
    else {
        setColor(COLOR_GREEN);
        cout << "HIT\n";
    }
    setColor(COLOR_DEFAULT);

    cout << "\n";

    // Borde superior
    cout << "+";
    for (int t = 0; t <= pasoActual; t++)
        cout << string(ancho, '-') << "+";
    cout << "\n";

    // Referencias
    cout << "|";
    for (int t = 0; t <= pasoActual; t++)
        cout << setw(ancho) << referencias[t] << "|";
    cout << "\n";

    // Marcos
    for (int i = 0; i < filas; i++) {
        cout << "|";
        for (int t = 0; t <= pasoActual; t++) {

            if (tabla[i][t] == -1) {
                setColor(COLOR_GRAY);
                cout << setw(ancho) << "-";
            }
            else {
                if (t == pasoActual)
                    setColor(COLOR_GREEN);
                else
                    setColor(COLOR_DEFAULT);

                cout << setw(ancho) << tabla[i][t];
            }

            setColor(COLOR_DEFAULT);
            cout << "|";
        }
        cout << "\n";
    }

    // Borde inferior
    cout << "+";
    for (int t = 0; t <= pasoActual; t++)
        cout << string(ancho, '-') << "+";
    cout << "\n";
}

void animarSimulacion(
    const vector<int>& referencias,
    const vector<vector<int>>& tabla,
    const vector<char>& estado)
{
    for (int paso = 0; paso < referencias.size(); paso++) {

        dibujarTablaAnimada(referencias, tabla, estado, paso);

        cout << "\nPagina ingresando: ";
        setColor(COLOR_GREEN);
        cout << referencias[paso] << "\n";
        setColor(COLOR_DEFAULT);

        this_thread::sleep_for(chrono::milliseconds(400));
    }

    cout << "\nSimulacion completa.\n";
    this_thread::sleep_for(chrono::milliseconds(1000));
}

int main() {

    int n, marcos, op;

    // =============================
    // VALIDACION DE ENTRADAS
    // =============================
    while (true) {
        cout << "Cantidad referencias (mayor que 0): ";
        if (cin >> n && n > 0) break;

        cin.clear();
        cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        cout << "Entrada invalida.\n";
    }

    while (true) {
        cout << "Numero marcos (mayor que 0 y menor o igual que referencias): ";
        if (cin >> marcos && marcos > 0 && marcos <= n) break;

        cin.clear();
        cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        cout << "Entrada invalida.\n";
    }

    vector<int> paginas = generarCadena(n);

    bool continuar = true;

    vector<Comparacion> resultados;

    // =========================================
    // FUNCION EJECUTAR (CON CONTROL ANIMACION)
    // =========================================
    auto ejecutar = [&](string nombre,
        Resultado(*algoritmo)(const vector<int>&, int),
        vector<Comparacion>& resultados,
        bool animar) {

            ULONGLONG intAntes = obtenerInterrupcionesSistema();

            FILETIME c1, e1, k1, u1;
            GetProcessTimes(GetCurrentProcess(), &c1, &e1, &k1, &u1);

            ULARGE_INTEGER kernelAntes;
            kernelAntes.LowPart = k1.dwLowDateTime;
            kernelAntes.HighPart = k1.dwHighDateTime;

            auto inicio = chrono::high_resolution_clock::now();
            Resultado r = algoritmo(paginas, marcos);
            auto fin = chrono::high_resolution_clock::now();

            ULONGLONG intDespues = obtenerInterrupcionesSistema();

            FILETIME c2, e2, k2, u2;
            GetProcessTimes(GetCurrentProcess(), &c2, &e2, &k2, &u2);

            ULARGE_INTEGER kernelDespues;
            kernelDespues.LowPart = k2.dwLowDateTime;
            kernelDespues.HighPart = k2.dwHighDateTime;

            double tiempo =
                chrono::duration<double, milli>(fin - inicio).count();

            Metricas m = obtenerMetricas(
                tiempo,
                intAntes,
                intDespues,
                kernelAntes.QuadPart,
                kernelDespues.QuadPart
            );

            cout << "\n====== " << nombre << " ======\n";
            cout << "Fallos: " << r.fallos << " | Hits: " << r.hits << "\n";

            // Solo animar si se solicita
            if (animar) {
                animarSimulacion(paginas, r.tabla, r.estado);
            }

            dibujarTablaHorizontal(paginas, r.tabla, r.estado);
            exportarCSV(nombre + ".csv", paginas, r.tabla, r.estado);
            exportarHTML(nombre + ".html", paginas, r.tabla, r.estado);

            mostrarMetricas(m);

            resultados.push_back({ nombre, r.fallos, r.hits, tiempo });
        };

    // =============================
    // LOOP PRINCIPAL
    // =============================
    while (continuar) {

        cout << "\n=====================================\n";
        cout << "1 FIFO\n";
        cout << "2 OPTIMO\n";
        cout << "3 LRU\n";
        cout << "4 LFU\n";
        cout << "5 NFU\n";
        cout << "6 CLOCK\n";
        cout << "7 SEGUNDA\n";
        cout << "8 NRU\n";
        cout << "9 TODOS\n";
        cout << "=====================================\n";

        do {
            cin >> op;
        } while (op < 1 || op > 9);

        resultados.clear();

        switch (op) {

        case 1: ejecutar("FIFO", FIFO, resultados, true); break;
        case 2: ejecutar("OPTIMO", OPTIMO, resultados, true); break;
        case 3: ejecutar("LRU", LRU, resultados, true); break;
        case 4: ejecutar("LFU", LFU, resultados, true); break;
        case 5: ejecutar("NFU", NFU, resultados, true); break;
        case 6: ejecutar("CLOCK", CLOCK, resultados, true); break;
        case 7: ejecutar("SEGUNDA", SEGUNDA, resultados, true); break;
        case 8: ejecutar("NRU", NRU, resultados, true); break;

        case 9:
            cout << "\nEjecutando todos los algoritmos con la misma secuencia...\n";

            ejecutar("FIFO", FIFO, resultados, false);
            ejecutar("OPTIMO", OPTIMO, resultados, false);
            ejecutar("LRU", LRU, resultados, false);
            ejecutar("LFU", LFU, resultados, false);
            ejecutar("NFU", NFU, resultados, false);
            ejecutar("CLOCK", CLOCK, resultados, false);
            ejecutar("SEGUNDA", SEGUNDA, resultados, false);
            ejecutar("NRU", NRU, resultados, false);
            break;
        }

        // =============================
        // CUADRO COMPARATIVO
        // =============================
        if (!resultados.empty()) {

            cout << "\n=============================================\n";
            cout << "           CUADRO COMPARATIVO\n";
            cout << "=============================================\n";

            cout << left << setw(12) << "Algoritmo"
                << setw(10) << "Fallos"
                << setw(10) << "Hits"
                << setw(15) << "Tiempo(ms)"
                << setw(12) << "HitRate(%)"
                << "\n";

            cout << "-------------------------------------------------------------\n";

            for (auto& r : resultados) {

                double hitRate =
                    (double)r.hits / (r.hits + r.fallos) * 100.0;

                cout << left << setw(12) << r.nombre
                    << setw(10) << r.fallos
                    << setw(10) << r.hits
                    << setw(15) << fixed << setprecision(3) << r.tiempo
                    << setw(12) << setprecision(2) << hitRate
                    << "\n";
            }

            // Ranking por menor cantidad de fallos
            sort(resultados.begin(), resultados.end(),
                [](const Comparacion& a, const Comparacion& b) {
                    return a.fallos < b.fallos;
                });

            cout << "\nRANKING (Menor cantidad de fallos)\n";

            for (int i = 0; i < resultados.size(); i++) {
                cout << i + 1 << "° "
                    << resultados[i].nombre
                    << " (" << resultados[i].fallos
                    << " fallos)\n";
            }

            cout << "=============================================\n";
        }

        cout << "\n¿Deseas probar otro algoritmo? (1=SI / 0=NO): ";
        cin >> continuar;

        if (continuar) {
            paginas = generarCadena(n);
            cout << "\nNueva secuencia generada.\n";
        }
    }

    return 0;
}
