#include "bound_process.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>


Ref<BoundProcess> BoundProcess::start(String p_path, PackedStringArray p_args)
{
    Ref<BoundProcess> process = memnew(BoundProcess);
    process->start_impl(p_path, p_args);
    return process;
}


bool BoundProcess::is_running()
{
    if (!started) { return false; }

    // Check to see if the child process has exited
    int pid = waitpid(child_pid, nullptr, WNOHANG);

    if (pid == 0)
    {
        // Still running
        return true;
    }
    else if (pid == child_pid)
    {
        // Child has stopped, clean up
        child_pid = 0;
        fclose(child_stdin); child_stdin = nullptr;
        fclose(child_stdout); child_stdout = nullptr;
        fclose(child_stderr); child_stderr = nullptr;
        started = false;

        return false;
    }
    else
    {
        // Error has occurred
        print_error("An error has occurred waiting on the child!");
        return false;
    }
}


void BoundProcess::kill_process()
{
    if (!started) { return; }

    // Destroy the child
    kill(child_pid, SIGTERM);

    child_pid = 0;
    fclose(child_stdin); child_stdin = nullptr;
    fclose(child_stdout); child_stdout = nullptr;
    fclose(child_stderr); child_stderr = nullptr;
    started = false;
}


Error BoundProcess::write_bytes(PackedByteArray p_bytes)
{
    if (!started) { return ERR_UNAVAILABLE; }

    size_t bytes_written = fwrite(p_bytes.ptr(), sizeof(uint8_t), p_bytes.size(), child_stdin);
    fflush(child_stdin);

    if (bytes_written == p_bytes.size())
    {
        return OK;
    }
    else
    {
        print_error(vformat("Only wrote %d out of %d bytes!", (unsigned int)bytes_written, p_bytes.size()));
        return FAILED;
    }
}


PackedByteArray BoundProcess::read_bytes(int p_size, ReadSource p_source, Array p_error)
{
    if (!started)
    {
        p_error.push_back(ERR_UNAVAILABLE);
        print_error("Cannot read bytes when BoundProcess is not running!");
        return {};
    }

    FILE* file;
    if (p_source == READ_STDOUT)
    {
        file = child_stdout;
    }
    else
    {
        file = child_stderr;
    }

    PackedByteArray bytes;
    bytes.resize_zeroed(p_size);

    size_t bytes_read = fread(bytes.ptrw(), sizeof(uint8_t), bytes.size(), file);

    if (bytes_read == bytes.size())
    {
        p_error.push_back(OK);
    }
    else
    {
        print_error(vformat("Only read %d out of %d bytes!", (unsigned int)bytes_read, bytes.size()));
        p_error.push_back(FAILED);
    }

    return bytes;
}


Error BoundProcess::write_line(String p_line)
{
    if (!started) { return ERR_UNAVAILABLE; }

    if (!p_line.ends_with("\n"))
    {
        p_line += "\n";
    }

    if (fputs(p_line.utf8().ptr(), child_stdin) < 0)
    {
        print_error("Write line failed!");
        return FAILED;
    }
    fflush(child_stdin);

    return OK;
}


String BoundProcess::read_line(ReadSource p_source, Array p_error)
{
    if (!started)
    {
        p_error.push_back(ERR_UNAVAILABLE);
        print_error("Cannot read line when BoundProcess is not running!");
        return "";
    }

    FILE* file;
    if (p_source == READ_STDOUT)
    {
        file = child_stdout;
    }
    else
    {
        file = child_stderr;
    }

    char buf[4096];
    memset(&buf[0], 0, 4096);

    if (fgets(&buf[0], 4096, file) == nullptr)
    {
        print_error("Read line failed!");
        p_error.push_back(ERR_UNAVAILABLE);
        return "";
    }
    else
    {
        p_error.push_back(OK);
    }

    return String::utf8(&buf[0], 4096).strip_edges(); // upper bound, not actual size
}


void BoundProcess::_bind_methods()
{
    BIND_ENUM_CONSTANT(READ_STDOUT);
    BIND_ENUM_CONSTANT(READ_STDERR);

    ClassDB::bind_static_method("BoundProcess", D_METHOD("start", "path", "args"), &BoundProcess::start);

    ClassDB::bind_method(D_METHOD("is_running"), &BoundProcess::is_running);
    ClassDB::bind_method(D_METHOD("kill_process"), &BoundProcess::kill_process);

    ClassDB::bind_method(D_METHOD("write_bytes", "bytes"), &BoundProcess::write_bytes);
    ClassDB::bind_method(D_METHOD("read_bytes", "size", "source", "error"), &BoundProcess::read_bytes, DEFVAL(READ_STDOUT), DEFVAL(Array()));

    ClassDB::bind_method(D_METHOD("write_line", "line"), &BoundProcess::write_line);
    ClassDB::bind_method(D_METHOD("read_line", "source", "error"), &BoundProcess::read_line, DEFVAL(READ_STDOUT), DEFVAL(Array()));
}


void BoundProcess::start_impl(String p_path, PackedStringArray p_args)
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    // Create pipes
    if (pipe(stdin_pipe) != 0)
    {
        print_error("Failed to create stdin pipe!");
        return;
    }
    if (pipe(stdout_pipe) != 0)
    {
        print_error("Failed to create stdout pipe!");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        return;
    }
    if (pipe(stderr_pipe) != 0)
    {
        print_error("Failed to create stderr pipe!");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        return;
    }

    // Open pipes as FILEs
    child_stdin = fdopen(stdin_pipe[1], "w"); // Open stdin writer
    if (child_stdin == nullptr)
    {
        print_error("Failed to open child stdin for writing!");
        close(stdin_pipe[0]); close(stdin_pipe[1]);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return;
    }
    child_stdout = fdopen(stdout_pipe[0], "r"); // Open stdout reader
    if (child_stdout == nullptr)
    {
        print_error("Failed to open child stdout for writing!");
        close(stdin_pipe[0]); fclose(child_stdin);
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return;
    }
    child_stderr = fdopen(stderr_pipe[0], "r"); // Open stderr reader
    if (child_stderr == nullptr)
    {
        print_error("Failed to open child stderr for writing!");
        close(stdin_pipe[0]); fclose(child_stdin);
        fclose(child_stdout); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return;
    }

    // Fork the process
    child_pid = fork();
    if (child_pid == -1)
    {
        print_error("Failed to fork BoundProcess!");
        close(stdin_pipe[0]); fclose(child_stdin);
        fclose(child_stdout); close(stdout_pipe[1]);
        fclose(child_stderr); close(stderr_pipe[1]);
        return;
    }

    if (child_pid == 0)
    {
        // Child process

        // Close parent end of pipes
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Bind stdin, out, and err
        if (stdin_pipe[0] != STDIN_FILENO)
        {
            dup2(stdin_pipe[0], STDIN_FILENO);
            close(stdin_pipe[0]);
        }
        if (stdout_pipe[1] != STDOUT_FILENO)
        {
            dup2(stdout_pipe[1], STDOUT_FILENO);
            close(stdout_pipe[1]);
        }
        if (stderr_pipe[1] != STDERR_FILENO)
        {
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stderr_pipe[1]);
        }

        // Prepare args (See os_unix.cpp::execute())
        Vector<CharString> cs;
        cs.push_back(p_path.utf8());
        for (int idx = 0; idx < p_args.size(); ++idx)
        {
            cs.push_back(p_args[idx].utf8());
        }

        Vector<char*> args;
        for (int idx = 0; idx < cs.size(); ++idx)
        {
            args.push_back(cs.write[idx].ptrw());
        }
        args.push_back(nullptr);

        CharString path = p_path.utf8();

        execvp(path.ptr(), &args.write[0]);

        print_error(vformat("Failed to exec child for BoundProcess %s", p_path));
        raise(SIGKILL);
    }
    else
    {
        // Parent process

        // Close child end of pipes
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        started = true;
    }
}


BoundProcess::~BoundProcess()
{
    kill_process();
}