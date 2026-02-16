#include "io/process.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace crosside::io {
namespace {

std::string buildDisplayCommand(const std::string &command, const std::vector<std::string> &args) {
    std::ostringstream cmd;
    cmd << shellQuote(command);
    for (const auto &arg : args) {
        cmd << ' ' << shellQuote(arg);
    }
    return cmd.str();
}

bool validateWorkingDirectory(const std::filesystem::path &cwd, const crosside::Context &ctx) {
    if (cwd.empty()) {
        return true;
    }

    std::error_code ec;
    if (!std::filesystem::exists(cwd, ec) || !std::filesystem::is_directory(cwd, ec)) {
        ctx.error("Working directory does not exist: ", cwd.string());
        return false;
    }
    return true;
}

#ifdef _WIN32

bool utf8ToWide(const std::string &input, std::wstring &out) {
    out.clear();
    if (input.empty()) {
        return true;
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return false;
    }

    out.resize(static_cast<std::size_t>(size));
    if (MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, out.data(), size) <= 0) {
        out.clear();
        return false;
    }
    return true;
}

ProcessResult runCommandWindows(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool detached
) {
    ProcessResult result;
    result.commandLine = buildDisplayCommand(command, args);

    std::wstring wideCmdLine;
    if (!utf8ToWide(result.commandLine, wideCmdLine)) {
        result.code = -1;
        ctx.error("Failed to convert command line to wide string");
        return result;
    }

    std::wstring wideCwd;
    wchar_t *cwdPtr = nullptr;
    if (!cwd.empty()) {
        if (!utf8ToWide(cwd.string(), wideCwd)) {
            result.code = -1;
            ctx.error("Failed to convert working directory to wide string");
            return result;
        }
        cwdPtr = wideCwd.data();
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    DWORD flags = 0;
    if (detached) {
        flags |= DETACHED_PROCESS;
        flags |= CREATE_NEW_PROCESS_GROUP;
    }

    BOOL ok = CreateProcessW(
        nullptr,
        wideCmdLine.data(),
        nullptr,
        nullptr,
        FALSE,
        flags,
        nullptr,
        cwdPtr,
        &si,
        &pi
    );

    if (!ok) {
        result.code = -1;
        ctx.error("Failed to create process: Error code ", static_cast<unsigned long>(GetLastError()));
        return result;
    }

    result.processId = static_cast<long long>(pi.dwProcessId);

    if (detached) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        result.code = 0;
        return result;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        result.code = static_cast<int>(exitCode);
    } else {
        result.code = -1;
        ctx.error("Failed to get process exit code");
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return result;
}

#else

std::vector<char *> makeArgv(std::vector<std::string> &storage) {
    std::vector<char *> argv;
    argv.reserve(storage.size() + 1);
    for (auto &item : storage) {
        argv.push_back(const_cast<char *>(item.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

ProcessResult runCommandPosix(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool detached
) {
    ProcessResult result;
    result.commandLine = buildDisplayCommand(command, args);

    std::vector<std::string> storage;
    storage.reserve(args.size() + 1);
    storage.push_back(command);
    storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char *> argv = makeArgv(storage);

    if (!detached) {
        const pid_t pid = fork();
        if (pid < 0) {
            result.code = -1;
            ctx.error("Failed to fork process: ", std::strerror(errno));
            return result;
        }

        if (pid == 0) {
            if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
                _exit(127);
            }
            execvp(command.c_str(), argv.data());
            _exit(127);
        }

        result.processId = static_cast<long long>(pid);
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            result.code = -1;
            ctx.error("Failed to wait for process: ", std::strerror(errno));
            return result;
        }

        if (WIFEXITED(status)) {
            result.code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result.code = 128 + WTERMSIG(status);
            ctx.warn("Process terminated by signal: ", WTERMSIG(status));
        } else {
            result.code = -1;
            ctx.error("Process ended abnormally");
        }
        return result;
    }

    const pid_t launcher = fork();
    if (launcher < 0) {
        result.code = -1;
        ctx.error("Failed to fork detached launcher: ", std::strerror(errno));
        return result;
    }

    if (launcher == 0) {
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            _exit(127);
        }

        if (setsid() < 0) {
            _exit(127);
        }

        const pid_t daemon = fork();
        if (daemon < 0) {
            _exit(127);
        }
        if (daemon > 0) {
            _exit(0);
        }

        const int devNull = open("/dev/null", O_RDWR);
        if (devNull >= 0) {
            dup2(devNull, STDIN_FILENO);
            dup2(devNull, STDOUT_FILENO);
            dup2(devNull, STDERR_FILENO);
            if (devNull > STDERR_FILENO) {
                close(devNull);
            }
        }

        execvp(command.c_str(), argv.data());
        _exit(127);
    }

    result.processId = static_cast<long long>(launcher);
    int launcherStatus = 0;
    if (waitpid(launcher, &launcherStatus, 0) < 0) {
        result.code = -1;
        ctx.error("Failed waiting detached launcher: ", std::strerror(errno));
        return result;
    }

    if (WIFEXITED(launcherStatus) && WEXITSTATUS(launcherStatus) == 0) {
        result.code = 0;
        return result;
    }

    result.code = -1;
    ctx.error("Detached launcher failed for command: ", command);
    return result;
}

#endif

ProcessResult runCommandInternal(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool dryRun,
    bool detached
) {
    ProcessResult result;
    result.commandLine = buildDisplayCommand(command, args);

    if (!cwd.empty()) {
        ctx.log("cwd: ", cwd.string());
    }
    ctx.log(result.commandLine);

    if (dryRun) {
        result.code = 0;
        return result;
    }

    if (!validateWorkingDirectory(cwd, ctx)) {
        result.code = -1;
        return result;
    }

#ifdef _WIN32
    return runCommandWindows(command, args, cwd, ctx, detached);
#else
    return runCommandPosix(command, args, cwd, ctx, detached);
#endif
}

} // namespace

std::string shellQuote(const std::string &value) {
#ifdef _WIN32
    if (value.empty()) {
        return "\"\"";
    }

    bool needQuotes = false;
    for (char ch : value) {
        if (ch == ' ' || ch == '\t' || ch == '"') {
            needQuotes = true;
            break;
        }
    }
    if (!needQuotes) {
        return value;
    }

    std::string out;
    out.push_back('"');
    std::size_t backslashes = 0;
    for (char ch : value) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
            continue;
        }
        out.append(backslashes, '\\');
        backslashes = 0;
        out.push_back(ch);
    }
    out.append(backslashes * 2, '\\');
    out.push_back('"');
    return out;
#else
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('\'');
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out.push_back(ch);
        }
    }
    out.push_back('\'');
    return out;
#endif
}

ProcessResult runCommand(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool dryRun
) {
    return runCommandInternal(command, args, cwd, ctx, dryRun, false);
}

ProcessResult runCommandDetached(
    const std::string &command,
    const std::vector<std::string> &args,
    const std::filesystem::path &cwd,
    const crosside::Context &ctx,
    bool dryRun
) {
    return runCommandInternal(command, args, cwd, ctx, dryRun, true);
}

std::optional<std::filesystem::path> currentExecutablePath() {
#ifdef _WIN32
    std::wstring widePath;
    widePath.resize(32768);
    const DWORD len = GetModuleFileNameW(nullptr, widePath.data(), static_cast<DWORD>(widePath.size()));
    if (len == 0 || len >= widePath.size()) {
        return std::nullopt;
    }
    widePath.resize(len);

    const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), static_cast<int>(widePath.size()), nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return std::nullopt;
    }
    std::string utf8Path(static_cast<std::size_t>(utf8Size), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, widePath.c_str(), static_cast<int>(widePath.size()), utf8Path.data(), utf8Size, nullptr, nullptr) <= 0) {
        return std::nullopt;
    }
    return std::filesystem::absolute(std::filesystem::path(utf8Path));
#else
    std::vector<char> buffer(4096);
    const ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len <= 0) {
        return std::nullopt;
    }
    buffer[static_cast<std::size_t>(len)] = '\0';
    return std::filesystem::absolute(std::filesystem::path(buffer.data()));
#endif
}

} // namespace crosside::io
