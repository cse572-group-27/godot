def can_build(env, platform):
    return (platform == "linuxbsd") or (platform == "macos")


def configure(env):
    pass

def get_doc_path():
    return "doc_classes"

def get_doc_classes():
    return ["BoundProcess"]