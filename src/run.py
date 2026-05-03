import argparse
import logging
import os
import signal
import wandb

from src.entities import KnownFormalism, KnownSolver
from src.exitcode import ExitCode
from src.slurmtimeout import slurm_timeout_signal_receiver
from src.loggerconfig import setup_logger


from src.pipes.pipe import MSPSP_Pipe
from src.pipes.pipe_class import get_pipe_class

setup_logger()
logger = logging.getLogger(__name__)

def make_argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="MSPSP experiment script")
    parser.add_argument(
        "--project-name",
        type=str,
        required=False,
        default="MSPSP",
        help="Name of the project where the experiment will be saved in wandb",
    )
    parser.add_argument(
        '--wandb-disable-stats',
        type=bool,
        default=True,
        required=False,
        action=argparse.BooleanOptionalAction,
        help="Disable wandb stats (machine information)",
    )
    parser.add_argument(
        "--mspsp-instance",
        type=str,
        required=True,
        help="Path to the MSPSP instance",
    )
    parser.add_argument(
        "--formalism",
        required=True,
        type=str,
        choices=[e.value for e in KnownFormalism],
        help="Formalism used for the experiment, e.g., OPB, MAXSAT, ...",
    )
    parser.add_argument(
        "--solver",
        required=True,
        type=str,
        choices=[e.value for e in KnownSolver],
        help="Solver identifier",
    )
    parser.add_argument(
        '--solving-timeout',
        type=int,
        default=600,
        required=False,
        help="Solving timeout in seconds",
    )
    parser.add_argument(
        '--db-yml',
        type=str,
        default='mspsp-db.yml',
        required=False,
        help='Path to the run database YAML file. In here we define the different scripts for the MSPSP experiments'
    )
    return parser


if __name__ == "__main__":
    # Partially parse the arguments
    parser = make_argument_parser()
    partial_args, _ = parser.parse_known_args()

    # Dynamically get the pipeline class (just the type)
    pipeline_cls: type[MSPSP_Pipe] = get_pipe_class(partial_args.formalism)
    pipeline_cls.register_args(parser)

    # Finish parsing the arguments with the pipeline-specific arguments
    args = parser.parse_args()

    # Instantiate the pipeline with the provided arguments
    pipe: MSPSP_Pipe = pipeline_cls(args)

    # Pipe information
    inline_config = pipe.inline_config()
    config = pipe.config()
    group = pipe.group()

    wandb_project = f'{args.project_name}@{inline_config}'
    wandb_dir = os.path.join("./.wandb", wandb_project)

    with wandb.init(
         project=wandb_project,
         dir=wandb_dir,
         config=config,
         group=group,
         settings=wandb.Settings(x_disable_stats=args.wandb_disable_stats),
    ) as run:
        logger.info(f"Initialized W&B run with ID: {run.id}")
        logger.info(f'\t\tDisabled wandb stats: {args.wandb_disable_stats}')

        # Handles the slurm timeout
        signal.signal(signal.SIGUSR1, slurm_timeout_signal_receiver(run, ExitCode.SUCCESS))

        # Run the pipeline
        pipe.run(run)

        # Indicates that the run did not timeout
        run.log({"slurm": {"timeout": False}})