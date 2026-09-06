#include <Windows.h>
#include <iostream>
#include <cstdio>

class ConsoleHelper {
public:
    ConsoleHelper() {
        // 1. 새로운 콘솔 창 할당
        if (AllocConsole()) {
            // 2. 표준 입출력 스트림을 콘솔로 리다이렉트
            freopen_s(&m_fpIn, "CONIN$", "r", stdin);
            freopen_s(&m_fpOut, "CONOUT$", "w", stdout);
            freopen_s(&m_fpErr, "CONOUT$", "w", stderr);

            // 3. C++ 표준 스트림 동기화 (cout, cin 사용 가능하게 함)
            std::ios::sync_with_stdio();
        }
    }

    ~ConsoleHelper() {
        // 4. 할당된 스트림 닫기 및 콘솔 해제
        if (m_fpIn)  fclose(m_fpIn);
        if (m_fpOut) fclose(m_fpOut);
        if (m_fpErr) fclose(m_fpErr);

        FreeConsole();
    }

    // 복사 방지 (RAII 객체 특성상 복사는 허용하지 않음)
    ConsoleHelper(const ConsoleHelper&) = delete;
    ConsoleHelper& operator=(const ConsoleHelper&) = delete;

private:
    FILE* m_fpIn = nullptr;
    FILE* m_fpOut = nullptr;
    FILE* m_fpErr = nullptr;
};