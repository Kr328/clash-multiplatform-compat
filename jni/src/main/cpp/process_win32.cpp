#include "process.hpp"

#include <windows.h>
#include <cstdlib>
#include <cstdint>
#include <memory>

#define PIPE_BUFFER_SIZE 4096

namespace process {
    class ScopedHandle {
    private:
        HANDLE handle = INVALID_HANDLE_VALUE;
    public:
        ScopedHandle &operator=(HANDLE newHandle) {
            this->handle = newHandle;
            return *this;
        }

        HANDLE get() {
            return handle;
        }

    public:
        ~ScopedHandle() {
            DWORD code = GetLastError();
            CloseHandle(handle);
            SetLastError(code);
        }
    };

    bool createPipePair(
            ScopedHandle &readable,
            ScopedHandle &writable,
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

    bool openNulDevice(ScopedHandle &handle) {
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
        std::for_each(args.begin(), args.end(), [&](const std::string &arg) {
            joinedArgs += "\"" + arg + "\" ";
        });

        std::string joinedEnvs;
        std::for_each(environments.begin(), environments.end(), [&](const std::string &env) {
            joinedEnvs += env;
            joinedEnvs += std::string("\0", 1);
        });

        ScopedHandle stdinReadable;
        ScopedHandle stdinWritable;
        if (fdStdin != nullptr) {
            if (!createPipePair(stdinReadable, stdinWritable, true, false)) {
                return false;
            }
        }

        ScopedHandle stdoutReadable;
        ScopedHandle stdoutWritable;
        if (fdStdout != nullptr) {
            if (!createPipePair(stdoutReadable, stdoutWritable, false, true)) {
                return false;
            }
        }

        ScopedHandle stderrReadable;
        ScopedHandle stderrWritable;
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

        using Attributes = LPPROC_THREAD_ATTRIBUTE_LIST;
        using AttributesDeleter = std::function<void(Attributes *)>;
        auto _attributesList = static_cast<Attributes>(malloc(attributesListSize));
        std::unique_ptr<Attributes, AttributesDeleter> attributesList = {
                &_attributesList,
                [](Attributes *v) { free(*v); }
        };
        if (!InitializeProcThreadAttributeList(*attributesList, 1, 0, &attributesListSize)) {
            return false;
        }

        ScopedHandle nul;
        if (!openNulDevice(nul)) {
            return false;
        }

        HANDLE childStdin = stdinReadable.get();
        if (childStdin == INVALID_HANDLE_VALUE) {
            childStdin = nul.get();
        }

        HANDLE childStdout = stdoutWritable.get();
        if (childStdout == INVALID_HANDLE_VALUE) {
            childStdout = nul.get();
        }

        HANDLE childStderr = stderrWritable.get();
        if (childStderr == INVALID_HANDLE_VALUE) {
            childStderr = nul.get();
        }

        HANDLE inheritHandles[3] = {childStdin, childStdout, childStderr};

        WINBOOL attributeUpdated = UpdateProcThreadAttribute(
                *attributesList,
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
        si.lpAttributeList = *attributesList;

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
            *fdStdin = stdinWritable.get();
            stdinWritable = INVALID_HANDLE_VALUE;
        }
        if (fdStdout != nullptr) {
            *fdStdout = stdoutReadable.get();
            stdoutReadable = INVALID_HANDLE_VALUE;
        }
        if (fdStderr != nullptr) {
            *fdStderr = stderrReadable.get();
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
