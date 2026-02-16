import subprocess


SHOW_COMMAND = False


def cmd_args_to_str(cmd_args):
    return " ".join([arg if " " not in arg else f'"{arg}"' for arg in cmd_args])


def runProcess(command, args=None, wait=True):
    if args is None:
        args = []

    cmd = [command] + args
    if SHOW_COMMAND:
        print("Execute -> ", cmd_args_to_str(cmd))

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    stdout, stderr = proc.communicate()
    if wait:
        proc.wait()
    return proc.returncode, stdout, stderr


def call_cmd(command, args=None):
    if args is None:
        args = []

    _ = [command] + args
    result = subprocess.Popen(
        [command],
        shell=True,
        close_fds=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    result.wait()
    chkdata = result.stdout.readlines()
    for line in chkdata:
        lstr = line.decode("utf-8")
        print(lstr)

