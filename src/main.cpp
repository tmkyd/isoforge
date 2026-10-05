// SPDX-License-Identifier: Apache-2.0

#include <string_view>
#include <vector>

#include "cli/app.h"
#include "cli/console.h"

int wmain(int argc, wchar_t* argv[]) {
    const std::vector<std::wstring_view> args(argv + 1, argv + argc);
    isoforge::cli::SystemConsole console;
    return isoforge::cli::run(args, console);
}
