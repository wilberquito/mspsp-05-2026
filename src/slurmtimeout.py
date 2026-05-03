import sys
import logging
import time
from typing import Any

from src.loggerconfig import setup_logger

setup_logger()
logger = logging.getLogger(__name__)


def slurm_timeout_signal_receiver(run: Any, exit_code: int=1, await_secs: int = 3):
    """Returns a signal handler that handles SLURM timeout warnings."""

    def _handle_slurm_timeout(signum: Any, frame: Any):
        logger.info("⚠️ Received SIGUSR1 (SLURM timeout warning). Finishing W&B run.")

        run.log( { "slurm": { "timeout": True } } )

        time.sleep(await_secs)

        run.finish(exit_code=exit_code)

        time.sleep(await_secs)

        # Exit the program
        sys.exit(exit_code)

    return _handle_slurm_timeout


