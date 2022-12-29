#include "process.hpp"

#include "utils.hpp"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>

namespace process {
    static void closeFd(int fd) {
        auto err = errno;
        close(fd);
        errno = err;
    }

    static bool createPipePair(utils::Scoped<int> &readable, utils::Scoped<int> &writable) {
        int pipeFds[2] = {-1, -1};

        if (pipe2(pipeFds, O_CLOEXEC) < 0) {
            return false;
        }

        readable = pipeFds[0];
        writable = pipeFds[1];

        return true;
    }

    static void cleanFileDescriptors(int fdExecutable) {
        DIR *fds = opendir("/proc/self/fd");
        if (fds == nullptr) {
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(fds)) != nullptr) {
            int fd = (int) strtol(entry->d_name, nullptr, 10);
            if (fd == dirfd(fds) || fd == fdExecutable ||
                fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
                continue;
            }
            close(fd);
        }

        closedir(fds);
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
        utils::Scoped<int> fdWorkingDir{open(workingDir.data(), O_RDONLY | O_DIRECTORY | O_CLOEXEC), closeFd};
        if (fdWorkingDir < 0) {
            return false;
        }

        utils::Scoped<int> fdExecutable{open(path.data(), O_RDONLY | O_CLOEXEC), closeFd};
        if (fdExecutable < 0) {
            return false;
        }

        std::string fdExecutablePath = "/proc/self/fd/" + std::to_string(static_cast<int>(fdExecutable));
        if (access(fdExecutablePath.data(), R_OK | X_OK) != 0) {
            return false;
        }

        utils::Scoped<int> fdStdinReadable{-1, closeFd};
        utils::Scoped<int> fdStdinWritable{-1, closeFd};
        if (fdStdin && !createPipePair(fdStdinReadable, fdStdinWritable)) {
            return false;
        }

        utils::Scoped<int> fdStdoutReadable{-1, closeFd};
        utils::Scoped<int> fdStdoutWritable{-1, closeFd};
        if (fdStdout && !createPipePair(fdStdoutReadable, fdStdoutWritable)) {
            return false;
        }

        utils::Scoped<int> fdStderrReadable{-1, closeFd};
        utils::Scoped<int> fdStderrWritable{-1, closeFd};
        if (fdStderr && !createPipePair(fdStderrReadable, fdStderrWritable)) {
            return false;
        }

        pid_t pid = fork();
        if (pid > 0) { // parent
            *handle = pid;

            if (fdStdin) {
                *fdStdin = fdStdinWritable;
                fdStdinWritable = -1;
            }
            if (fdStdout) {
                *fdStdout = fdStdoutReadable;
                fdStdoutReadable = -1;
            }
            if (fdStderr) {
                *fdStderr = fdStderrReadable;
                fdStderrReadable = -1;
            }

            return true;
        } else if (pid == 0) { // child
            if (fchdir(fdWorkingDir) < 0) {
                abort();
            }

            int fdNull = open("/dev/null", O_WRONLY | O_CLOEXEC);

            if (!fdStdin) {
                fdStdinReadable = fdNull;
            }
            if (!fdStdout) {
                fdStdoutWritable = fdNull;
            }
            if (!fdStderr) {
                fdStderrWritable = fdNull;
            }

            if (dup3(fdStdinReadable, STDIN_FILENO, 0) < 0) {
                abort();
            }
            if (dup3(fdStdoutWritable, STDOUT_FILENO, 0) < 0) {
                abort();
            }
            if (dup3(fdStderrWritable, STDERR_FILENO, 0) < 0) {
                abort();
            }

            cleanFileDescriptors(fdExecutable);

            std::vector<const char *> cArgs;
            for (const auto &arg: args) {
                cArgs.push_back(arg.data());
            }
            cArgs.push_back(nullptr);

            std::vector<const char *> cEnvironments;
            for (const auto &env: environments) {
                cEnvironments.push_back(env.data());
            }
            cEnvironments.push_back(nullptr);

            if (fexecve(
                    fdExecutable,
                    const_cast<char *const *>(cArgs.data()),
                    const_cast<char *const *>(cEnvironments.data())
            ) < 0) {
                abort();
            }

            return -1;
        }

        return -1;
    }

    int wait(ResourceHandle handle) {
        auto pid = static_cast<pid_t>(handle);
        int r = -1;
        waitpid(pid, &r, 0);
        return r;
    }

    void terminate(ResourceHandle handle) {
        auto pid = static_cast<pid_t>(handle);
        kill(pid, SIGKILL);
    }

    void release(ResourceHandle handle) {
        // do nothing
    }
}
