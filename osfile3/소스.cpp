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

//2-1, 2-2 �κ������� �߽��ϴ�....
using namespace std;

// ���μ����� ������ ���¸� ��Ÿ���� ������
enum ProcessType { FG, BG };
enum ProcessState { READY, RUNNING, WAITING };

// ���μ��� ����ü
struct Process {
    int id;
    ProcessType type;
    ProcessState state;
    int waitTime; // ��� �ð��� ��Ÿ��
    bool promoted; // ���θ�� ���θ� ��Ÿ��

    Process(int id, ProcessType type, ProcessState state = READY, int waitTime = 0, bool promoted = false)
        : id(id), type(type), state(state), waitTime(waitTime), promoted(promoted) {}
};

// ���� ���� ����
atomic<bool> running(true);
condition_variable cv;
mutex mtx;
queue<Process> DynamicQueue;
vector<Process> WaitQueue;
Process* runningProcess = nullptr;
int nextPid = 0; // FG, BG ���� ���� PID ����

// �Լ� ����
void scheduler();
void shell();
void monitor();
void printStatus();
string processToString(const Process& p);
vector<string> parse(const string& command);
void exec(const vector<string>& args);

// �����ٷ� �Լ�: ���μ����� �����ϰ� ���� ���¸� ������Ʈ��
void scheduler() {
    int time = 0;
    while (running) {
        this_thread::sleep_for(chrono::seconds(1));
        unique_lock<mutex> lock(mtx);

        // ��� �ð��� ���� ���μ������� ����
        auto it = remove_if(WaitQueue.begin(), WaitQueue.end(), [&](Process& p) {
            if (p.waitTime <= time) {
                p.promoted = true; // ��� ���μ����� ���θ�ǵ�
                DynamicQueue.push(p);
                return true;
            }
            return false;
            });
        WaitQueue.erase(it, WaitQueue.end());

        // ���� ���� ���� ���μ����� ���ų�, FG ���μ����� ������ �����ٸ�
        if (!runningProcess || (!DynamicQueue.empty() && DynamicQueue.front().type == FG)) {
            if (runningProcess) {
                runningProcess->state = READY;
                DynamicQueue.push(*runningProcess);
                delete runningProcess; // �޸� ����
            }
            runningProcess = new Process(DynamicQueue.front());
            runningProcess->state = RUNNING;
            DynamicQueue.pop();
        }

        time++;
        cv.notify_all();
    }
}

// �� �Լ�: ����� ��ɾ �޾� ó����
void shell() {
    while (running) {
        this_thread::sleep_for(chrono::seconds(2)); // ��ɾ� �߰� �ֱ�
        unique_lock<mutex> lock(mtx);
        cv.wait(lock); // �����ٷ��� �˸� ���

        const string command = "run_process"; // ���� ��ɾ�
        vector<string> args = parse(command);
        exec(args);

        // FG/BG �����ư��� ���μ��� �߰�
        ProcessType type = nextPid % 2 == 0 ? FG : BG;
        DynamicQueue.push(Process(nextPid++, type));
        cout << "Shell: Adding " << (type == FG ? "FG" : "BG") << " Process " << nextPid - 1 << endl;
        printStatus();
    }
}

// ����� �Լ�: �ý��� ���¸� ���� �ð����� �����
void monitor() {
    while (running) {
        this_thread::sleep_for(chrono::seconds(5)); // ���� �ð����� ���� ���
        unique_lock<mutex> lock(mtx);
        cout << "Monitor: Current System Status" << endl;
        printStatus();
    }
}

// ��ɾ �Ľ��ϴ� �Լ�
vector<string> parse(const string& command) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(command);
    while (getline(tokenStream, token, ' ')) {
        tokens.push_back(token);
    }
    return tokens;
}

// ��ɾ �����ϴ� �Լ�
void exec(const vector<string>& args) {
    // ��ɾ� ���� �ùķ��̼�
    cout << "Executing command: ";
    for (const auto& arg : args) {
        cout << arg << " ";
    }
    cout << endl;
}

// ���μ����� ���ڿ��� ��ȯ�ϴ� �Լ�
string processToString(const Process& p) {
    string result = "[" + to_string(p.id) + (p.type == FG ? "F" : "B");
    if (p.promoted) {
        result += "*"; // ���θ�ǵ� ���μ��� ǥ��
    }
    if (p.state == WAITING) {
        result += ":" + to_string(p.waitTime); // WQ�� �ִ� ���μ����� ���� �ð� ǥ��
    }
    result += "]";
    return result;
}

// ���� �ý��� ���¸� ����ϴ� �Լ�
void printStatus() {
    // ù ��° Running ���� ��� (bottom ����)
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
        while (!tempQueue.empty()) {
            cout << processToString(tempQueue.front()) << " ";
            tempQueue.pop();
        }
        cout << "(bottom/top)" << endl;
    }
    else {
        cout << "[] (bottom/top)" << endl;
    }
    cout << "---------------------------" << endl;

    // Wait Queue ���� ���
    cout << "WQ: ";
    if (!WaitQueue.empty()) {
        for (const auto& p : WaitQueue) {
            cout << processToString(p) << " ";
        }
    }
    else {
        cout << "[]";
    }
    cout << endl;
    cout << "---------------------------" << endl;

    // �� ��° Running ���� ��� (bottom �߰�)
    cout << "Running: ";
    if (runningProcess) {
        cout << processToString(*runningProcess) << " (bottom)" << endl;
    }
    else {
        cout << "[]" << " (bottom)" << endl;
    }
    cout << "---------------------------" << endl;
}

// ���� �Լ�
int main() {
    // �ʱ� ���μ��� ����
    DynamicQueue.push(Process(nextPid++, FG));
    DynamicQueue.push(Process(nextPid++, BG));

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

    delete runningProcess; // runningProcess �޸� ����

    return 0;
}
