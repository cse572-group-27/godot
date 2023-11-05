#pragma once

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"


class BoundProcess : public RefCounted
{
    GDCLASS(BoundProcess, RefCounted);

public:
    enum ReadSource
    {
        READ_STDOUT,
        READ_STDERR
    };

    static Ref<BoundProcess> start(String p_path, PackedStringArray p_args);

    bool is_running();
    void kill_process();

    Error write_bytes(PackedByteArray p_bytes);
    PackedByteArray read_bytes(int p_size, ReadSource p_source = READ_STDOUT, Array p_error = Array());

    Error write_line(String p_line);
    String read_line(ReadSource p_source = READ_STDOUT, Array p_error = Array());

protected:
    static void _bind_methods();

private:
    int child_pid = 0;
    FILE* child_stdin = nullptr;
    FILE* child_stdout = nullptr;
    FILE* child_stderr = nullptr;
    bool started = false;

    void start_impl(String p_path, PackedStringArray p_args);

public:
    BoundProcess() {}
    ~BoundProcess();
};

VARIANT_ENUM_CAST(BoundProcess::ReadSource);