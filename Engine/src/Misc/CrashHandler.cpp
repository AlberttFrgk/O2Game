#include "Misc/CrashHandler.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#if _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "version.lib")
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace {
    std::atomic<bool> g_isHandlingCrash{ false };
    std::mutex g_logMutex;
    std::deque<std::string> g_recentLogs;
    constexpr size_t MAX_LOG_HISTORY = 100;

    std::string g_lastCrashLogPath;
    std::string g_lastCrashDumpPath;

    std::string GetCurrentTimestamp(bool forFileName = false)
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#if _WIN32
        localtime_s(&tmNow, &in_time_t);
#else
        localtime_r(&in_time_t, &tmNow);
#endif
        std::ostringstream ss;
        if (forFileName) {
            ss << std::put_time(&tmNow, "%Y-%m-%d_%H-%M-%S");
        } else {
            ss << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S");
        }
        return ss.str();
    }

#if _WIN32
    std::string WideToUtf8(const wchar_t *wstr)
    {
        if (!wstr) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
        if (size <= 0) return "";
        std::string str(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
        return str;
    }

    std::wstring Utf8ToWide(const std::string &str)
    {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (size <= 0) return L"";
        std::wstring wstr(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
        return wstr;
    }

    std::string GetExceptionDescription(DWORD code)
    {
        switch (code) {
            case EXCEPTION_ACCESS_VIOLATION:         return "EXCEPTION_ACCESS_VIOLATION (0xC0000005)";
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED (0xC000008C)";
            case EXCEPTION_BREAKPOINT:               return "EXCEPTION_BREAKPOINT (0x80000003)";
            case EXCEPTION_DATATYPE_MISALIGNMENT:    return "EXCEPTION_DATATYPE_MISALIGNMENT (0x80000002)";
            case EXCEPTION_FLT_DENORMAL_OPERAND:     return "EXCEPTION_FLT_DENORMAL_OPERAND (0xC000008D)";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:       return "EXCEPTION_FLT_DIVIDE_BY_ZERO (0xC000008E)";
            case EXCEPTION_FLT_INEXACT_RESULT:       return "EXCEPTION_FLT_INEXACT_RESULT (0xC000008F)";
            case EXCEPTION_FLT_INVALID_OPERATION:    return "EXCEPTION_FLT_INVALID_OPERATION (0xC0000090)";
            case EXCEPTION_FLT_OVERFLOW:             return "EXCEPTION_FLT_OVERFLOW (0xC0000091)";
            case EXCEPTION_FLT_STACK_CHECK:          return "EXCEPTION_FLT_STACK_CHECK (0xC0000092)";
            case EXCEPTION_FLT_UNDERFLOW:            return "EXCEPTION_FLT_UNDERFLOW (0xC0000093)";
            case EXCEPTION_ILLEGAL_INSTRUCTION:      return "EXCEPTION_ILLEGAL_INSTRUCTION (0xC000001D)";
            case EXCEPTION_IN_PAGE_ERROR:            return "EXCEPTION_IN_PAGE_ERROR (0xC0000006)";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:       return "EXCEPTION_INT_DIVIDE_BY_ZERO (0xC0000094)";
            case EXCEPTION_INT_OVERFLOW:             return "EXCEPTION_INT_OVERFLOW (0xC0000095)";
            case EXCEPTION_INVALID_DISPOSITION:      return "EXCEPTION_INVALID_DISPOSITION (0xC0000026)";
            case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION (0xC0000025)";
            case EXCEPTION_PRIV_INSTRUCTION:         return "EXCEPTION_PRIV_INSTRUCTION (0xC0000096)";
            case EXCEPTION_SINGLE_STEP:              return "EXCEPTION_SINGLE_STEP (0x80000004)";
            case EXCEPTION_STACK_OVERFLOW:           return "EXCEPTION_STACK_OVERFLOW (0xC00000FD)";
            case 0xE06D7363:                         return "MSVC_CPP_EXCEPTION (0xE06D7363)";
            default: {
                char buf[64];
                snprintf(buf, sizeof(buf), "UNKNOWN_EXCEPTION (0x%08lX)", code);
                return buf;
            }
        }
    }

    std::string GetModuleFromAddress(void *addr, DWORD64 &offset)
    {
        HMODULE hMod = NULL;
        offset = 0;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &hMod)) {
            char modName[MAX_PATH] = { 0 };
            GetModuleFileNameA(hMod, modName, MAX_PATH);
            offset = (DWORD64)addr - (DWORD64)hMod;
            std::filesystem::path p(modName);
            return p.filename().string();
        }
        return "UnknownModule";
    }

    std::string GetWindowsVersionString()
    {
        typedef NTSTATUS(WINAPI * RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
            if (pRtlGetVersion) {
                RTL_OSVERSIONINFOW rovi = { 0 };
                rovi.dwOSVersionInfoSize = sizeof(rovi);
                if (pRtlGetVersion(&rovi) == 0) {
                    std::ostringstream ss;
                    ss << "Windows " << rovi.dwMajorVersion << "." << rovi.dwMinorVersion
                       << " (Build " << rovi.dwBuildNumber << ")";
                    return ss.str();
                }
            }
        }
        return "Windows";
    }

    void CaptureCallStack(HANDLE hProcess, HANDLE hThread, CONTEXT *pCtx, std::vector<std::string> &frames)
    {
        SymInitialize(hProcess, NULL, TRUE);
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);

        CONTEXT context = {};
        if (pCtx) {
            context = *pCtx;
        } else {
            RtlCaptureContext(&context);
        }

        STACKFRAME64 stackFrame = {};
        DWORD machineType = IMAGE_FILE_MACHINE_AMD64;

#if defined(_M_X64) || defined(__x86_64__)
        stackFrame.AddrPC.Offset = context.Rip;
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Offset = context.Rbp;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Offset = context.Rsp;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        machineType = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86) || defined(__i386__)
        stackFrame.AddrPC.Offset = context.Eip;
        stackFrame.AddrPC.Mode = AddrModeFlat;
        stackFrame.AddrFrame.Offset = context.Ebp;
        stackFrame.AddrFrame.Mode = AddrModeFlat;
        stackFrame.AddrStack.Offset = context.Esp;
        stackFrame.AddrStack.Mode = AddrModeFlat;
        machineType = IMAGE_FILE_MACHINE_I386;
#endif

        int frameIndex = 0;
        char symBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(char)] = { 0 };
        PSYMBOL_INFO pSym = (PSYMBOL_INFO)symBuffer;
        pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSym->MaxNameLen = MAX_SYM_NAME;

        while (frameIndex < 64 && StackWalk64(machineType, hProcess, hThread, &stackFrame, &context,
                                              NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
            DWORD64 addr = stackFrame.AddrPC.Offset;
            if (addr == 0) break;

            DWORD64 modOffset = 0;
            std::string modName = GetModuleFromAddress((void *)addr, modOffset);

            std::string symbolName = "unknown";
            DWORD64 symDisplacement = 0;
            if (SymFromAddr(hProcess, addr, &symDisplacement, pSym)) {
                symbolName = pSym->Name;
            }

            IMAGEHLP_LINE64 lineInfo = {};
            lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;
            std::string location;
            if (SymGetLineFromAddr64(hProcess, addr, &lineDisplacement, &lineInfo)) {
                location = std::string(lineInfo.FileName) + ":" + std::to_string(lineInfo.LineNumber);
            }

            std::ostringstream ss;
            ss << "[" << std::setw(2) << std::setfill('0') << frameIndex << "] "
               << "0x" << std::hex << std::setw(16) << std::setfill('0') << addr << std::dec << " in "
               << modName << "!" << symbolName;
            if (!location.empty()) {
                ss << " (" << location << ")";
            }
            frames.push_back(ss.str());
            frameIndex++;
        }

        if (frames.empty()) {
            void *backtraceAddrs[64];
            USHORT count = CaptureStackBackTrace(0, 64, backtraceAddrs, NULL);
            for (USHORT i = 0; i < count; i++) {
                DWORD64 addr = (DWORD64)backtraceAddrs[i];
                DWORD64 modOffset = 0;
                std::string modName = GetModuleFromAddress((void *)addr, modOffset);

                std::string symbolName = "unknown";
                DWORD64 symDisplacement = 0;
                if (SymFromAddr(hProcess, addr, &symDisplacement, pSym)) {
                    symbolName = pSym->Name;
                }

                IMAGEHLP_LINE64 lineInfo = {};
                lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                DWORD lineDisplacement = 0;
                std::string location;
                if (SymGetLineFromAddr64(hProcess, addr, &lineDisplacement, &lineInfo)) {
                    location = std::string(lineInfo.FileName) + ":" + std::to_string(lineInfo.LineNumber);
                }

                std::ostringstream ss;
                ss << "[" << std::setw(2) << std::setfill('0') << i << "] "
                   << "0x" << std::hex << std::setw(16) << std::setfill('0') << addr << std::dec << " in "
                   << modName << "!" << symbolName;
                if (!location.empty()) {
                    ss << " (" << location << ")";
                }
                frames.push_back(ss.str());
            }
        }
    }

    void DumpLoadedModules(HANDLE hProcess, std::vector<std::string> &modules)
    {
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W me = { 0 };
            me.dwSize = sizeof(me);
            if (Module32FirstW(hSnapshot, &me)) {
                do {
                    std::ostringstream ss;
                    ss << "0x" << std::hex << std::setw(16) << std::setfill('0') << (DWORD64)me.modBaseAddr
                       << " - 0x" << std::setw(16) << std::setfill('0') << (DWORD64)(me.modBaseAddr + me.modBaseSize)
                       << std::dec << " : " << std::filesystem::path(me.szExePath).filename().string()
                       << " (" << std::filesystem::path(me.szExePath).string() << ")";
                    modules.push_back(ss.str());
                } while (Module32NextW(hSnapshot, &me));
            }
            CloseHandle(hSnapshot);
        }
    }

    void WriteMinidump(EXCEPTION_POINTERS *pExInfo, const std::filesystem::path &dumpPath)
    {
        HANDLE hFile = CreateFileW(dumpPath.wstring().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION exInfo = {};
            exInfo.ThreadId = GetCurrentThreadId();
            exInfo.ExceptionPointers = pExInfo;
            exInfo.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal,
                              pExInfo ? &exInfo : NULL, NULL, NULL);
            CloseHandle(hFile);
        }
    }
#endif
}

namespace CrashHandler {

    void AddLogMessage(const std::string &message)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_recentLogs.size() >= MAX_LOG_HISTORY) {
            g_recentLogs.pop_front();
        }
        g_recentLogs.push_back(message);
    }

    std::string GetLastCrashLogPath()
    {
        return g_lastCrashLogPath;
    }

    std::string GetLastCrashDumpPath()
    {
        return g_lastCrashDumpPath;
    }

    void WriteCrashReport(const std::string &reason, void *exceptionPointers)
    {
        if (g_isHandlingCrash.exchange(true)) {
            return;
        }

        std::string timeStr = GetCurrentTimestamp();
        std::string fileTimeStr = GetCurrentTimestamp(true);

        std::filesystem::path logsDir = std::filesystem::current_path() / "Logs";
        std::error_code ec;
        std::filesystem::create_directories(logsDir, ec);

        std::filesystem::path crashLogLatest = std::filesystem::current_path() / "crash.log";
        std::filesystem::path crashLogTimestamped = logsDir / ("crash_" + fileTimeStr + ".log");
        std::filesystem::path crashDumpLatest = std::filesystem::current_path() / "crash.dmp";
        std::filesystem::path crashDumpTimestamped = logsDir / ("crash_" + fileTimeStr + ".dmp");

        g_lastCrashLogPath = crashLogLatest.string();
        g_lastCrashDumpPath = crashDumpLatest.string();

        std::ostringstream report;
        report << "O2Game Crash Report\n";
        report << "Timestamp: " << timeStr << "\n";
        report << "Reason: " << reason << "\n";
#if _WIN32
        report << "Operating System: " << GetWindowsVersionString() << "\n";
        report << "Process ID: " << GetCurrentProcessId() << "\n";
        report << "Thread ID: " << GetCurrentThreadId() << "\n";
        report << "Architecture: x64\n\n";

        EXCEPTION_POINTERS *pExInfo = static_cast<EXCEPTION_POINTERS *>(exceptionPointers);
        DWORD exCode = 0;
        void *faultAddr = nullptr;
        std::string exDesc;
        std::string exDetails;
        std::string faultModule;
        DWORD64 faultModuleOffset = 0;

        if (pExInfo && pExInfo->ExceptionRecord) {
            exCode = pExInfo->ExceptionRecord->ExceptionCode;
            faultAddr = pExInfo->ExceptionRecord->ExceptionAddress;
            exDesc = GetExceptionDescription(exCode);
            faultModule = GetModuleFromAddress(faultAddr, faultModuleOffset);

            if (exCode == EXCEPTION_ACCESS_VIOLATION && pExInfo->ExceptionRecord->NumberParameters >= 2) {
                ULONG_PTR accessType = pExInfo->ExceptionRecord->ExceptionInformation[0];
                ULONG_PTR targetAddr = pExInfo->ExceptionRecord->ExceptionInformation[1];
                std::string opStr;
                if (accessType == 0) opStr = "reading from";
                else if (accessType == 1) opStr = "writing to";
                else if (accessType == 8) opStr = "executing at";
                else opStr = "accessing";

                char buf[128];
                snprintf(buf, sizeof(buf), "Attempted %s address 0x%016llX", opStr.c_str(), (unsigned long long)targetAddr);
                exDetails = buf;
            }
        } else {
            exDesc = reason;
        }

        report << "Exception: " << exDesc << "\n";
        if (faultAddr) {
            report << "Fault Address: 0x" << std::hex << std::setw(16) << std::setfill('0')
                   << (DWORD64)faultAddr << std::dec << "\n";
            report << "Fault Module: " << faultModule << " (+0x" << std::hex << faultModuleOffset << std::dec << ")\n";
        }
        if (!exDetails.empty()) {
            report << "Details: " << exDetails << "\n";
        }

        CONTEXT localCtx = {};
        CONTEXT *ctx = (pExInfo && pExInfo->ContextRecord) ? pExInfo->ContextRecord : nullptr;
        if (!ctx) {
            RtlCaptureContext(&localCtx);
            ctx = &localCtx;
        }

        if (ctx) {
            report << "\nRegisters:\n";
#if defined(_M_X64) || defined(__x86_64__)
            report << "RAX: 0x" << std::hex << std::setw(16) << std::setfill('0') << ctx->Rax << "  "
                   << "RBX: 0x" << std::setw(16) << std::setfill('0') << ctx->Rbx << "  "
                   << "RCX: 0x" << std::setw(16) << std::setfill('0') << ctx->Rcx << "\n";
            report << "RDX: 0x" << std::setw(16) << std::setfill('0') << ctx->Rdx << "  "
                   << "RSI: 0x" << std::setw(16) << std::setfill('0') << ctx->Rsi << "  "
                   << "RDI: 0x" << std::setw(16) << std::setfill('0') << ctx->Rdi << "\n";
            report << "RBP: 0x" << std::setw(16) << std::setfill('0') << ctx->Rbp << "  "
                   << "RSP: 0x" << std::setw(16) << std::setfill('0') << ctx->Rsp << "  "
                   << "RIP: 0x" << std::setw(16) << std::setfill('0') << ctx->Rip << "\n";
            report << "R8:  0x" << std::setw(16) << std::setfill('0') << ctx->R8  << "  "
                   << "R9:  0x" << std::setw(16) << std::setfill('0') << ctx->R9  << "  "
                   << "R10: 0x" << std::setw(16) << std::setfill('0') << ctx->R10 << "\n";
            report << "R11: 0x" << std::setw(16) << std::setfill('0') << ctx->R11 << "  "
                   << "R12: 0x" << std::setw(16) << std::setfill('0') << ctx->R12 << "  "
                   << "R13: 0x" << std::setw(16) << std::setfill('0') << ctx->R13 << "\n";
            report << "R14: 0x" << std::setw(16) << std::setfill('0') << ctx->R14 << "  "
                   << "R15: 0x" << std::setw(16) << std::setfill('0') << ctx->R15 << "  "
                   << "EFLAGS: 0x" << std::setw(8) << std::setfill('0') << ctx->EFlags << std::dec << "\n";
#endif
        }

        report << "\nCall Stack:\n";
        std::vector<std::string> callstack;
        CaptureCallStack(GetCurrentProcess(), GetCurrentThread(), pExInfo ? pExInfo->ContextRecord : NULL, callstack);
        if (callstack.empty()) {
            report << "  (No call stack frames captured)\n";
        } else {
            for (const auto &line : callstack) {
                report << "  " << line << "\n";
            }
        }

        report << "\nRecent Logs:\n";
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            if (g_recentLogs.empty()) {
                report << "  (No recent log messages)\n";
            } else {
                for (const auto &log : g_recentLogs) {
                    report << "  " << log << "\n";
                }
            }
        }

        report << "\nLoaded Modules:\n";
        std::vector<std::string> modules;
        DumpLoadedModules(GetCurrentProcess(), modules);
        for (const auto &mod : modules) {
            report << "  " << mod << "\n";
        }

        WriteMinidump(pExInfo, crashDumpLatest);
        WriteMinidump(pExInfo, crashDumpTimestamped);

#else
        report << "\nRecent Logs:\n";
        {
            std::lock_guard<std::mutex> lock(g_logMutex);
            for (const auto &log : g_recentLogs) {
                report << "  " << log << "\n";
            }
        }
#endif

        std::string fullReport = report.str();
        {
            std::ofstream fLatest(crashLogLatest, std::ios::out | std::ios::trunc);
            if (fLatest.is_open()) {
                fLatest << fullReport;
            }
            std::ofstream fTimestamped(crashLogTimestamped, std::ios::out | std::ios::trunc);
            if (fTimestamped.is_open()) {
                fTimestamped << fullReport;
            }
        }

        std::cerr << fullReport << std::endl;

#if _WIN32
        std::wostringstream wmsg;
        wmsg << L"A fatal error occurred.\n\n";

        if (!exDesc.empty()) {
            wmsg << L"Exception: " << Utf8ToWide(exDesc) << L"\n";
        }
        if (faultAddr) {
            wmsg << L"Fault Module: " << Utf8ToWide(faultModule)
                 << L" (+0x" << std::hex << faultModuleOffset << std::dec << L")\n";
        }
        if (!exDetails.empty()) {
            wmsg << L"Details: " << Utf8ToWide(exDetails) << L"\n";
        }

        wmsg << L"\nCrash report saved to:\n" << crashLogLatest.wstring() << L"\n"
             << L"\nMemory dump saved to:\n" << crashDumpLatest.wstring();

        MessageBoxW(NULL, wmsg.str().c_str(), L"O2Game Error",
                    MB_ICONERROR | MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
#endif
    }

#if _WIN32
    LONG WINAPI UnhandledCrashFilter(EXCEPTION_POINTERS *pExInfo)
    {
        std::string desc = "Unhandled exception";
        if (pExInfo && pExInfo->ExceptionRecord) {
            desc = GetExceptionDescription(pExInfo->ExceptionRecord->ExceptionCode);
        }
        WriteCrashReport(desc, pExInfo);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void CrtInvalidParameterHandler(const wchar_t *expression, const wchar_t *function,
                                    const wchar_t *file, unsigned int line, uintptr_t)
    {
        std::ostringstream ss;
        ss << "Invalid parameter";
        if (expression) {
            ss << ": " << WideToUtf8(expression);
        }
        if (function) {
            ss << " in function " << WideToUtf8(function);
        }
        if (file) {
            ss << " at " << WideToUtf8(file) << ":" << line;
        }

        CONTEXT ctx = {};
        RtlCaptureContext(&ctx);
        EXCEPTION_RECORD rec = {};
        rec.ExceptionCode = STATUS_INVALID_PARAMETER;
        rec.ExceptionAddress = (void *)ctx.Rip;
        EXCEPTION_POINTERS exInfo = { &rec, &ctx };

        WriteCrashReport(ss.str(), &exInfo);
        ExitProcess(STATUS_INVALID_PARAMETER);
    }

    void CrtPureCallHandler()
    {
        CONTEXT ctx = {};
        RtlCaptureContext(&ctx);
        EXCEPTION_RECORD rec = {};
        rec.ExceptionCode = STATUS_NONCONTINUABLE_EXCEPTION;
        rec.ExceptionAddress = (void *)ctx.Rip;
        EXCEPTION_POINTERS exInfo = { &rec, &ctx };

        WriteCrashReport("Pure virtual function call", &exInfo);
        ExitProcess(1);
    }
#endif

    void TerminateHandler()
    {
        std::string what = "std::terminate";
        std::exception_ptr ex = std::current_exception();
        if (ex) {
            try {
                std::rethrow_exception(ex);
            } catch (const std::exception &e) {
                what = std::string("Uncaught C++ exception: ") + e.what();
            } catch (...) {
                what = "Uncaught unknown C++ exception";
            }
        }

#if _WIN32
        CONTEXT ctx = {};
        RtlCaptureContext(&ctx);
        EXCEPTION_RECORD rec = {};
        rec.ExceptionCode = 0xE06D7363;
        rec.ExceptionAddress = (void *)ctx.Rip;
        EXCEPTION_POINTERS exInfo = { &rec, &ctx };
        WriteCrashReport(what, &exInfo);
        ExitProcess(1);
#else
        WriteCrashReport(what, nullptr);
        std::abort();
#endif
    }

    void SignalHandler(int signum)
    {
        std::string sigName = "Signal " + std::to_string(signum);
        if (signum == SIGABRT) sigName = "SIGABRT (Abort)";
        else if (signum == SIGSEGV) sigName = "SIGSEGV (Segmentation Fault)";
        else if (signum == SIGILL) sigName = "SIGILL (Illegal Instruction)";
        else if (signum == SIGFPE) sigName = "SIGFPE (Floating Point Exception)";
        else if (signum == SIGTERM) sigName = "SIGTERM (Termination Request)";

#if _WIN32
        CONTEXT ctx = {};
        RtlCaptureContext(&ctx);
        EXCEPTION_RECORD rec = {};
        rec.ExceptionCode = (DWORD)signum;
        rec.ExceptionAddress = (void *)ctx.Rip;
        EXCEPTION_POINTERS exInfo = { &rec, &ctx };
        WriteCrashReport(sigName, &exInfo);
        ExitProcess((UINT)signum);
#else
        WriteCrashReport(sigName, nullptr);
        std::abort();
#endif
    }

    void Install()
    {
        std::set_terminate(TerminateHandler);
        std::signal(SIGABRT, SignalHandler);
        std::signal(SIGSEGV, SignalHandler);
        std::signal(SIGILL, SignalHandler);
        std::signal(SIGFPE, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

#if _WIN32
        SetUnhandledExceptionFilter(UnhandledCrashFilter);
        _set_invalid_parameter_handler(CrtInvalidParameterHandler);
        _set_purecall_handler(CrtPureCallHandler);
#endif
    }

    void Uninstall()
    {
#if _WIN32
        SetUnhandledExceptionFilter(NULL);
        _set_invalid_parameter_handler(NULL);
        _set_purecall_handler(NULL);
#endif
    }
}
