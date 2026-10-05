// SPDX-License-Identifier: Apache-2.0

#include "cli/services.h"

#include <windows.h>

#include <atomic>

namespace isoforge::cli {
namespace {

// Windows ends the process about 5 seconds after a close event, or as soon as the handler
// returns, so the handler waits slightly less for the command to clean up.
constexpr DWORD kCloseCleanupWaitMs = 4500;

std::atomic<bool> interrupt_requested{false};

// Signalled when the command has finished (InterruptHandler's destructor). Created once and kept
// for the life of the process because a handler thread may still be waiting on it.
HANDLE cleanup_done() {
    static const HANDLE event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    return event;
}

BOOL WINAPI handle_control(DWORD type) {
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        interrupt_requested = true;
        return TRUE;
    case CTRL_CLOSE_EVENT:
        // Returning ends the process at once; give the command time to delete its partial file.
        interrupt_requested = true;
        if (cleanup_done() != nullptr) {
            ::WaitForSingleObject(cleanup_done(), kCloseCleanupWaitMs);
        }
        return TRUE;
    default:
        return FALSE;
    }
}

}  // namespace

InterruptHandler::InterruptHandler() {
    interrupt_requested = false;
    if (cleanup_done() != nullptr) {
        ::ResetEvent(cleanup_done());
    }
    ::SetConsoleCtrlHandler(handle_control, TRUE);
}

InterruptHandler::~InterruptHandler() {
    if (cleanup_done() != nullptr) {
        ::SetEvent(cleanup_done());
    }
    ::SetConsoleCtrlHandler(handle_control, FALSE);
}

bool InterruptHandler::requested() noexcept {
    return interrupt_requested;
}

}  // namespace isoforge::cli
