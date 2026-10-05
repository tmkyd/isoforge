// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace isoforge::cli {

// Destination of user-facing output. Tests substitute an in-memory implementation.
class Console {
public:
    virtual ~Console() = default;
    virtual void write_out(std::wstring_view text) = 0;
    virtual void write_err(std::wstring_view text) = 0;
    // True when standard error is an interactive console (progress lines use a carriage return).
    virtual bool err_is_terminal() const { return false; }
    // True when ANSI colors may be written to standard output or standard error.
    virtual bool out_has_color() const { return false; }
    virtual bool err_has_color() const { return false; }
};

// Writes to the process's standard output and standard error. A console receives UTF-16 through
// WriteConsoleW; a redirected handle (pipe or file) receives UTF-8. Colors are used on a console
// whose virtual terminal processing could be enabled, unless NO_COLOR is set; the console modes
// are restored on destruction.
class SystemConsole final : public Console {
public:
    SystemConsole();
    ~SystemConsole() override;
    SystemConsole(const SystemConsole&) = delete;
    SystemConsole& operator=(const SystemConsole&) = delete;

    void write_out(std::wstring_view text) override;
    void write_err(std::wstring_view text) override;
    bool err_is_terminal() const override;
    bool out_has_color() const override { return out_color_; }
    bool err_has_color() const override { return err_color_; }

private:
    // The console mode to restore, when this object changed it.
    std::optional<std::uint32_t> out_mode_;
    std::optional<std::uint32_t> err_mode_;
    bool out_color_ = false;
    bool err_color_ = false;
};

}  // namespace isoforge::cli
