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
    int waitTime; // 대기 시간을 나타내기 위해 추가
    bool promoted; // 프로모션 여부를 나타내기 위해 추가

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
        // 깨어날 시간이 지난 프로세스들을 깨움
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
    int id = 2; // 초기 값 2로 설정 (0, 1은 이미 추가됨)
    while (running) {
        this_thread::sleep_for(chrono::seconds(2)); // 명령어 추가 주기
        unique_lock<mutex> lock(mtx);
        cv.wait(lock);  // 스케줄러의 알림 대기

        const string command = "run_process";
        vector<string> args = parse(command);
        exec(args);

        if (id % 2 == 0) { // 예제 명령어 추가 (FG/BG 번갈아가며 추가)
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
        this_thread::sleep_for(chrono::seconds(5));  // 일정 시간마다 상태 출력
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
    tokens.push_back(""); // 마지막 원소로 길이 0인 문자열 추가
    return tokens;
}

void exec(const vector<string>& args) {
    // 명령어 실행 시뮬레이션
    cout << "Executing command: ";
    for (const auto& arg : args) {
        if (arg.empty()) break; // 빈 문자열을 만난 경우 종료
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
    // 첫 번째 Running 상태 출력
    cout << "Running: ";
    if (runningProcess) {
        cout << processToString(*runningProcess);
    }
    else {
        cout << "[]";
    }
    cout << endl;
    cout << "---------------------------" << endl;

    // Dynamic Queue 상태 출력
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

    // Wait Queue 상태 출력
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

    // 두 번째 Running 상태 출력
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
    // 초기 프로세스 설정
    DynamicQueue.push(Process(0, FG));
    DynamicQueue.push(Process(1, BG));

    // 스레드 생성
    thread schedulerThread(scheduler);
    thread shellThread(shell);
    thread monitorThread(monitor);

    // 일정 시간 후 프로그램 종료
    this_thread::sleep_for(chrono::seconds(20));
    running = false;

    // 스레드 종료 대기
    schedulerThread.join();
    shellThread.join();
    monitorThread.join();

    return 0;
}
