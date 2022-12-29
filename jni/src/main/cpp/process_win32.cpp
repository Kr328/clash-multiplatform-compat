#include "process.hpp"

#include "utils.hpp"

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <windows.h>

#define PIPE_BUFFER_SIZE 4096

namespace process {
    static void closeHandle(HANDLE handle) {
        DWORD code = GetLastError();
        CloseHandle(handle);
        SetLastError(code);
    }

    bool createPipePair(
            utils::Scoped<HANDLE> &readable,
            utils::Scoped<HANDLE> &writable,
            bool readableInheritable,
            bool writableInheritable
    ) {
        HANDLE _readable, _writable;
        if (!CreatePipe(&_readable, &_writable, nullptr, PIPE_BUFFER_SIZE)) {
            return false;
        }

        if (readableInheritable) {
            SetHandleInformation(_readable, HANDLE_FLAG_INHERIT, TRUE);
        }
        if (writableInheritable) {
            SetHandleInformation(_writable, HANDLE_FLAG_INHERIT, TRUE);
        }

        readable = _readable;
        writable = _writable;

        return true;
    }

    bool openNulDevice(utils::Scoped<HANDLE> &handle) {
        SECURITY_ATTRIBUTES attributes;
        memset(&attributes, 0, sizeof(SECURITY_ATTRIBUTES));
        attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
        attributes.bInheritHandle = TRUE;
        attributes.lpSecurityDescriptor = nullptr;

        HANDLE h = CreateFileA(
                "nul:",
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                &attributes,
                OPEN_EXISTING,
                0,
                nullptr
        );
        if (h == INVALID_HANDLE_VALUE) {
            return false;
        }

        handle = h;

        return true;
    }

    bool create(
            const std::string &path,
            const std::vector<std::string> &args,
            const std::string &workingDir,
            const std::vector<std::string> &environments,
            ResourceHandle *handle,
            ResourceHandle *fdStdin,
            ResourceHandle *fdStdout,
            ResourceHandle *fdStderr
    ) {
        std::string joinedArgs;
        for (const auto &arg: args) {
            joinedArgs += "\"" + arg + "\" ";
        }

        std::string joinedEnvs;
        for (const auto &env: environments) {
            joinedEnvs += env;
            joinedEnvs += std::string("\0", 1);
        }

        utils::Scoped<HANDLE> stdinReadable{InvalidResourceHandle, closeHandle};
        utils::Scoped<HANDLE> stdinWritable{InvalidResourceHandle, closeHandle};
        if (fdStdin != nullptr) {
            if (!createPipePair(stdinReadable, stdinWritable, true, false)) {
                return false;
            }
        }

        utils::Scoped<HANDLE> stdoutReadable{InvalidResourceHandle, closeHandle};
        utils::Scoped<HANDLE> stdoutWritable{InvalidResourceHandle, closeHandle};
        if (fdStdout != nullptr) {
            if (!createPipePair(stdoutReadable, stdoutWritable, false, true)) {
                return false;
            }
        }

        utils::Scoped<HANDLE> stderrReadable{InvalidResourceHandle, closeHandle};
        utils::Scoped<HANDLE> stderrWritable{InvalidResourceHandle, closeHandle};
        if (fdStderr != nullptr) {
            if (!createPipePair(stderrReadable, stderrWritable, false, true)) {
                return false;
            }
        }

        SIZE_T attributesListSize = 0;
        if (!InitializeProcThreadAttributeList(nullptr, 1, 0, &attributesListSize) &&
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return false;
        }

        utils::Scoped<LPPROC_THREAD_ATTRIBUTE_LIST> attributesList{
            static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attributesListSize)),
            free,
        };
        if (!InitializeProcThreadAttributeList(attributesList, 1, 0, &attributesListSize)) {
            return false;
        }

        utils::Scoped<HANDLE> nul{InvalidResourceHandle, closeHandle};
        if (!openNulDevice(nul)) {
            return false;
        }

        HANDLE childStdin = stdinReadable;
        if (childStdin == INVALID_HANDLE_VALUE) {
            childStdin = nul;
        }

        HANDLE childStdout = stdoutWritable;
        if (childStdout == INVALID_HANDLE_VALUE) {
            childStdout = nul;
        }

        HANDLE childStderr = stderrWritable;
        if (childStderr == INVALID_HANDLE_VALUE) {
            childStderr = nul;
        }

        HANDLE inheritHandles[3] = {childStdin, childStdout, childStderr};

        WINBOOL attributeUpdated = UpdateProcThreadAttribute(
                attributesList,
                0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                inheritHandles,
                sizeof(inheritHandles),
                nullptr,
                nullptr
        );
        if (!attributeUpdated) {
            return false;
        }

        STARTUPINFOEXA si;
        memset(&si, 0, sizeof(si));
        si.StartupInfo.cb = sizeof(si);
        si.StartupInfo.hStdInput = childStdin;
        si.StartupInfo.hStdOutput = childStdout;
        si.StartupInfo.hStdError = childStderr;
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.lpAttributeList = attributesList;

        PROCESS_INFORMATION info;
        memset(&info, 0, sizeof(info));

        LRESULT r = CreateProcessA(
                path.data(),
                joinedArgs.data(),
                nullptr,
                nullptr,
                TRUE,
                0,
                joinedEnvs.data(),
                workingDir.data(),
                &si.StartupInfo,
                &info
        );
        if (!r) {
            return false;
        }

        if (fdStdin != nullptr) {
            *fdStdin = stdinWritable;
            stdinWritable = INVALID_HANDLE_VALUE;
        }
        if (fdStdout != nullptr) {
            *fdStdout = stdoutReadable;
            stdoutReadable = INVALID_HANDLE_VALUE;
        }
        if (fdStderr != nullptr) {
            *fdStderr = stderrReadable;
            stderrReadable = INVALID_HANDLE_VALUE;
        }

        CloseHandle(info.hThread);
        SetLastError(ERROR_SUCCESS);

        *handle = info.hProcess;

        return true;
    }

    int wait(ResourceHandle handle) {
        DWORD code = 1;
        while (GetExitCodeProcess(handle, &code) && code == STILL_ACTIVE) {
            WaitForSingleObject(handle, INFINITE);
        }

        return (int) code;
    }

    void terminate(ResourceHandle handle) {
        TerminateProcess(handle, 255);
    }

    void release(ResourceHandle handle) {
        CloseHandle(handle);
    }
}
