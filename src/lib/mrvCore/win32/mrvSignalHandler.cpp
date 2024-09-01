

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <filesystem>

#include <windows.h>

namespace fs = std::filesystem;

#include "mrvCore/mrvHome.h"
#include "mrvCore/mrvSignalHandler.h"
#include "mrvCore/mrvStackTrace.h"

namespace mrv
{

    SignalHandler::SignalHandler()
    {
        install_signal_handler();
    }

    SignalHandler::~SignalHandler()
    {
        restore_signal_handler();
    }

    void signal_callback(int signal)
    {
        std::cerr << "GOT SIGNAL " << signal << std::endl;

        printStackTrace();

        exit(1);
    }

    LONG WINAPI exceptionHandler(EXCEPTION_POINTERS* exceptionInfo)
    {
        DWORD exceptionCode = exceptionInfo->ExceptionRecord->ExceptionCode;
        std::cerr << "Exception " << exceptionCode << " caught" << std::endl;
        // Do any necessary cleanup or logging here
        printStackTrace();

        return EXCEPTION_EXECUTE_HANDLER;
    }

    void SignalHandler::install_signal_handler()
    {
        // Set up exception handler
        SetUnhandledExceptionFilter(exceptionHandler);

        // Set up signal handlers
        std::signal(SIGSEGV, signal_callback);
        std::signal(SIGABRT, signal_callback);
#ifndef NDEBUG
        std::signal(SIGINT, signal_callback);
#endif
    }

    void SignalHandler::restore_signal_handler() {}

} // namespace mrv
