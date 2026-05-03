import time
import subprocess
import shlex
import logging
import shutil

from typing import Union, List, Optional, Dict, Any, Callable, Tuple
from pathlib import Path

from src.loggerconfig import setup_logger

setup_logger()

logger = logging.getLogger(__name__)

def timed_subrun(name: str, subrun: Callable[..., Any], *args: Tuple[Any, ], **kwargs: dict[str, Any]) -> Tuple[Any, float]:
    logger.debug(f"Timing: {name}")

    # Run an compute the elapsed time of the subrun
    start = time.time()
    result = subrun(*args, **kwargs)
    elapsed = time.time() - start

    logger.debug(f"Timing: {name} - Elapsed: {elapsed:.3f} secs")

    return result, elapsed


# Alias para paths que acepten str o Path/bytes
StrOrBytesPath = Union[str, bytes, Path]

def cmd_with_linux_time(
    cmd: Union[str, List[str]],
    *,
    cwd: Optional[StrOrBytesPath] = None,
    env: Optional[Dict[str, str]] = None,
    stdout: Optional[Any] = None,  # subprocess.PIPE or file-like object
    text: bool = True,
    timeout: Optional[float] = None
) -> dict:
    """
    Run a command under /usr/bin/time and return:
      - real/user/sys times
      - stdout/stderr of the command
      - exit code

    Args:
        cmd: list[str] or str — the command to run (list preferred)
        cwd: optional working directory
        env: optional environment variables
        text: decode bytes into str

    Returns:
        dict with keys:
            - real_time (float, seconds)
            - user_time (float, seconds)
            - sys_time  (float, seconds)
            - returncode (int)
            - stdout (str)
            - stderr (str)
    """

    TIME_BIN = shutil.which("gtime") or shutil.which("time") or "/usr/bin/time"

    format_str = "REAL:%e\tUSER:%U\tSYS:%S"

    if isinstance(cmd, str):
        cmd = shlex.split(cmd)

    # Transforming any non string into string
    cmd = [str(c) for c in cmd]

    if timeout is not None:
        full_cmd = [TIME_BIN, "-f", format_str, "timeout", f"{timeout}s", *cmd]
    else:
        full_cmd = [TIME_BIN, "-f", format_str, *cmd]

    out_handle = stdout if stdout is not None else subprocess.PIPE

    logger.debug(f"💲 {' '.join(shlex.quote(c) for c in full_cmd)}")

    result = subprocess.run(
        full_cmd,
        cwd=cwd,
        env=env,
        stdout=out_handle,
        stderr=subprocess.PIPE,
        text=text,
        close_fds=True,
        check=False,
    )

    result_stdout: str = result.stdout or ""
    result_stderr: str = result.stderr or ""
    returncode: int = result.returncode

    # Something went wrong if < 0
    real = user = sys = -1.0
    for part in result_stderr.split():
        if part.startswith("REAL:"):
            real = float(part.split("REAL:")[1].split("\t")[0])
        elif part.startswith("USER:"):
            user = float(part.split("USER:")[1].split("\t")[0])
        elif part.startswith("SYS:"):
            sys = float(part.split("SYS:")[1].split("\t")[0])

    return {
        "real_time": real,
        "user_time": user,
        "sys_time": sys,
        "returncode": returncode,
        "stdout": result_stdout,
        "stderr": result_stderr
    }
