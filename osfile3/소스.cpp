#include <iostream>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>

using namespace std;

enum ProcessType { FG, BG };

struct Process {
    int id;
    ProcessType type;
    int waitTime; // ��� �ð��� ��Ÿ���� ���� �߰�
    bool promoted; // ���θ�� ���θ� ��Ÿ���� ���� �߰�

    Process(int id, ProcessType type, int waitTime = 0, bool promoted = false)
        : id(id), type(type), waitTime(waitTime), promoted(promoted) {}
};

atomic<bool> running(true);
condition_variable cv;
mutex mtx;
queue<Process> DynamicQueue;
vector<Process> WaitQueue;
Process* runningProcess = nullptr;

void scheduler();
void shell();
void monitor();
void printStatus();
string processToString(const Process& p);
vector<string> parse(const string& command);
void exec(const vector<string>& args);

void scheduler() {
    int time = 0;
    while (running) {
        this_thread::sleep_for(chrono::seconds(1));
        unique_lock<mutex> lock(mtx);
        // ��� �ð��� ���� ���μ������� ����
        auto it = remove_if(WaitQueue.begin(), WaitQueue.end(), [&](Process& p) {
            if (p.waitTime <= time) {
                DynamicQueue.push(p);
                return true;
            }
            return false;
            });
        WaitQueue.erase(it, WaitQueue.end());

        if (!DynamicQueue.empty()) {
            runningProcess = new Process(DynamicQueue.front());
            DynamicQueue.pop();
        }
        else {
            runningProcess = nullptr;
        }
        time++;
        cv.notify_all();
    }
}

void shell() {
    int id = 2; // �ʱ� �� 2�� ���� (0, 1�� �̹� �߰���)
    while (running) {
        this_thread::sleep_for(chrono::seconds(2)); // ��ɾ� �߰� �ֱ�
        unique_lock<mutex> lock(mtx);
        cv.wait(lock);  // �����ٷ��� �˸� ���

        const string command = "run_process";
        vector<string> args = parse(command);
        exec(args);

        if (id % 2 == 0) { // ���� ��ɾ� �߰� (FG/BG �����ư��� �߰�)
            DynamicQueue.push(Process(id, FG));
            cout << "Shell: Adding FG Process " << id << endl;
        }
        else {
            DynamicQueue.push(Process(id, BG));
            cout << "Shell: Adding BG Process " << id << endl;
        }
        id++;
        printStatus();
    }
}

void monitor() {
    while (running) {
        this_thread::sleep_for(chrono::seconds(5));  // ���� �ð����� ���� ���
        unique_lock<mutex> lock(mtx);
        cout << "Monitor: Current System Status" << endl;
        printStatus();
    }
}

vector<string> parse(const string& command) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(command);
    while (getline(tokenStream, token, ' ')) {
        tokens.push_back(token);
    }
    tokens.push_back(""); // ������ ���ҷ� ���� 0�� ���ڿ� �߰�
    return tokens;
}

void exec(const vector<string>& args) {
    // ��ɾ� ���� �ùķ��̼�
    cout << "Executing command: ";
    for (const auto& arg : args) {
        if (arg.empty()) break; // �� ���ڿ��� ���� ��� ����
        cout << arg << " ";
    }
    cout << endl;
}

string processToString(const Process& p) {
    string result = "[" + to_string(p.id) + (p.type == FG ? "F" : "B");
    if (p.promoted) {
        result += "*";
    }
    result += "]";
    return result;
}

void printStatus() {
    // ù ��° Running ���� ���
    cout << "Running: ";
    if (runningProcess) {
        cout << processToString(*runningProcess);
    }
    else {
        cout << "[]";
    }
    cout << endl;
    cout << "---------------------------" << endl;

    // Dynamic Queue ���� ���
    cout << "DQ: ";
    if (!DynamicQueue.empty()) {
        cout << "P => ";
        queue<Process> tempQueue = DynamicQueue;
        vector<Process> processes;
        while (!tempQueue.empty()) {
            processes.push_back(tempQueue.front());
            tempQueue.pop();
        }
        for (size_t i = 0; i < processes.size(); ++i) {
            cout << processToString(processes[i]) << " ";
        }
        cout << "(bottom/top)" << endl;
    }
    else {
        cout << "P => [] (bottom/top)" << endl;
    }
    cout << "---------------------------" << endl;

    // Wait Queue ���� ���
    cout << "WQ: ";
    if (!WaitQueue.empty()) {
        for (const auto& p : WaitQueue) {
            cout << processToString(p) << ":" << p.waitTime << " ";
        }
    }
    else {
        cout << "[]";
    }
    cout << endl;
    cout << "---------------------------" << endl;

    // �� ��° Running ���� ���
    cout << "Running: ";
    if (runningProcess) {
        cout << processToString(*runningProcess) << " (bottom)" << endl;
    }
    else {
        cout << "[]" << " (bottom)" << endl;
    }
    cout << "---------------------------" << endl;
}




int main() {
    // �ʱ� ���μ��� ����
    DynamicQueue.push(Process(0, FG));
    DynamicQueue.push(Process(1, BG));

    // ������ ����
    thread schedulerThread(scheduler);
    thread shellThread(shell);
    thread monitorThread(monitor);

    // ���� �ð� �� ���α׷� ����
    this_thread::sleep_for(chrono::seconds(20));
    running = false;

    // ������ ���� ���
    schedulerThread.join();
    shellThread.join();
    monitorThread.join();

    return 0;
}
