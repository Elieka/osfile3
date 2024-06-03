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

//2-1, 2-2 부분적으로 했습니다....
using namespace std;

// 프로세스의 종류와 상태를 나타내는 열거형
enum ProcessType { FG, BG };
enum ProcessState { READY, RUNNING, WAITING };

// 프로세스 구조체
struct Process {
    int id;
    ProcessType type;
    ProcessState state;
    int waitTime; // 대기 시간을 나타냄
    bool promoted; // 프로모션 여부를 나타냄

    Process(int id, ProcessType type, ProcessState state = READY, int waitTime = 0, bool promoted = false)
        : id(id), type(type), state(state), waitTime(waitTime), promoted(promoted) {}
};

// 전역 변수 선언
atomic<bool> running(true);
condition_variable cv;
mutex mtx;
queue<Process> DynamicQueue;
vector<Process> WaitQueue;
Process* runningProcess = nullptr;
int nextPid = 0; // FG, BG 구분 없이 PID 생성

// 함수 선언
void scheduler();
void shell();
void monitor();
void printStatus();
string processToString(const Process& p);
vector<string> parse(const string& command);
void exec(const vector<string>& args);

// 스케줄러 함수: 프로세스를 관리하고 실행 상태를 업데이트함
void scheduler() {
    int time = 0;
    while (running) {
        this_thread::sleep_for(chrono::seconds(1));
        unique_lock<mutex> lock(mtx);

        // 깨어날 시간이 지난 프로세스들을 깨움
        auto it = remove_if(WaitQueue.begin(), WaitQueue.end(), [&](Process& p) {
            if (p.waitTime <= time) {
                p.promoted = true; // 깨어난 프로세스는 프로모션됨
                DynamicQueue.push(p);
                return true;
            }
            return false;
            });
        WaitQueue.erase(it, WaitQueue.end());

        // 현재 실행 중인 프로세스가 없거나, FG 프로세스가 있으면 스케줄링
        if (!runningProcess || (!DynamicQueue.empty() && DynamicQueue.front().type == FG)) {
            if (runningProcess) {
                runningProcess->state = READY;
                DynamicQueue.push(*runningProcess);
                delete runningProcess; // 메모리 해제
            }
            runningProcess = new Process(DynamicQueue.front());
            runningProcess->state = RUNNING;
            DynamicQueue.pop();
        }

        time++;
        cv.notify_all();
    }
}

// 셸 함수: 사용자 명령어를 받아 처리함
void shell() {
    while (running) {
        this_thread::sleep_for(chrono::seconds(2)); // 명령어 추가 주기
        unique_lock<mutex> lock(mtx);
        cv.wait(lock); // 스케줄러의 알림 대기

        const string command = "run_process"; // 예제 명령어
        vector<string> args = parse(command);
        exec(args);

        // FG/BG 번갈아가며 프로세스 추가
        ProcessType type = nextPid % 2 == 0 ? FG : BG;
        DynamicQueue.push(Process(nextPid++, type));
        cout << "Shell: Adding " << (type == FG ? "FG" : "BG") << " Process " << nextPid - 1 << endl;
        printStatus();
    }
}

// 모니터 함수: 시스템 상태를 일정 시간마다 출력함
void monitor() {
    while (running) {
        this_thread::sleep_for(chrono::seconds(5)); // 일정 시간마다 상태 출력
        unique_lock<mutex> lock(mtx);
        cout << "Monitor: Current System Status" << endl;
        printStatus();
    }
}

// 명령어를 파싱하는 함수
vector<string> parse(const string& command) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(command);
    while (getline(tokenStream, token, ' ')) {
        tokens.push_back(token);
    }
    return tokens;
}

// 명령어를 실행하는 함수
void exec(const vector<string>& args) {
    // 명령어 실행 시뮬레이션
    cout << "Executing command: ";
    for (const auto& arg : args) {
        cout << arg << " ";
    }
    cout << endl;
}

// 프로세스를 문자열로 변환하는 함수
string processToString(const Process& p) {
    string result = "[" + to_string(p.id) + (p.type == FG ? "F" : "B");
    if (p.promoted) {
        result += "*"; // 프로모션된 프로세스 표시
    }
    if (p.state == WAITING) {
        result += ":" + to_string(p.waitTime); // WQ에 있는 프로세스는 남은 시간 표시
    }
    result += "]";
    return result;
}

// 현재 시스템 상태를 출력하는 함수
void printStatus() {
    // 첫 번째 Running 상태 출력 (bottom 없음)
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

    // Wait Queue 상태 출력
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

    // 두 번째 Running 상태 출력 (bottom 추가)
    cout << "Running: ";
    if (runningProcess) {
        cout << processToString(*runningProcess) << " (bottom)" << endl;
    }
    else {
        cout << "[]" << " (bottom)" << endl;
    }
    cout << "---------------------------" << endl;
}

// 메인 함수
int main() {
    // 초기 프로세스 설정
    DynamicQueue.push(Process(nextPid++, FG));
    DynamicQueue.push(Process(nextPid++, BG));

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

    delete runningProcess; // runningProcess 메모리 해제

    return 0;
}
