from abc import ABC, abstractmethod
from enum import Enum
import logging
import yaml
from wandb.sdk.wandb_run import Run

from src.entities import KnownFormalism
from src.loggerconfig import setup_logger

setup_logger()

class SolutionStatus(Enum):
    OPTIMUM_FOUND = "OPTIMUM_FOUND"
    UNSATISFIABLE = "UNSATISFIABLE"
    SATISFIABLE = "SATISFIABLE"
    UNKNOWN = "UNKNOWN"
    ERROR = "ERROR"

class MSPSP_Pipe(ABC):
    def __init__(self, args):
        self.logger = logging.getLogger(
            f"{self.__class__.__module__}.{self.__class__.__name__}"
        )
        self.logger.info(f"Initializing {self.__class__.__name__}")

        # Read db content
        self._db_yml = args.db_yml
        self._db_data = self.__read_db()

        # Solving configuration
        self._mspsp_instance = args.mspsp_instance
        self._formalism = args.formalism
        self._solver = args.solver
        self._solving_timeout_s = args.solving_timeout

        # Scripts
        self._solving_script = self.__get_solving_script()

        # Solution status (error represents a failure in the pipeline)
        self._solution_status = None
        self._makespan = None
        self._model = None

        # W&B logger
        self.__wandb_run: Run | None = None

    @staticmethod
    def register_args(parser) -> None:
        pass

    def run(self, wandb_run: Run):
        self.__wandb_run = wandb_run

        run_steps = [
            self._log_configuration_step,
            self.__before_solving_step,
            self.__solving_step,
            self.__obtain_solution_status_step,
            self.__after_solving_step,
        ]

        for step in run_steps:
            try:
                data = step()
                self.logger.debug(f"{step.__name__} completed successfully.")
                if data is not None:
                    self.__wandb_run.log(data)
            except Exception as e:
                self.logger.error(f"Error during {step.__name__}: {str(e)}")
                self._solution_status = SolutionStatus.ERROR
                break

        self.__shutdown_run()

    def config(self) -> dict:
        return {
            "mspsp_instance": self._mspsp_instance,
            "formalism": self._formalism,
            "solver": self._solver,
            "solving_timeout_s": self._solving_timeout_s,
            "inline_config": self.inline_config()
        }

    def inline_config(self) -> str:
        return f"f={self._formalism}__s={self._solver}"

    def group(self) -> str:
        instance_parent = self._mspsp_instance.split("/")[-2]
        return instance_parent.lower()

    @abstractmethod
    def _db_key(self) -> str:
        pass

    @abstractmethod
    def _log_configuration_step(self) -> None:
        pass

    @abstractmethod
    def _before_solving_step(self) -> dict | None:
        pass

    @abstractmethod
    def _solving_step(self) -> dict | None:
        pass

    @abstractmethod
    def _after_solving_step(self) -> dict | None:
        pass

    @abstractmethod
    def _get_solution_status(self) -> SolutionStatus:
        pass

    @abstractmethod
    def _allocate_temp_files(self) -> None:
        pass

    @abstractmethod
    def _free_temp_files(self) -> None:
        pass

    def _metrics(self, result: dict, prefix: str) -> dict:
        time_metrics = {k: result[k] for k in ["real_time", "user_time", "sys_time"]}
        return {prefix: time_metrics}

    def _verify_result(self, result: dict, error_message: str, goal: int = 0):
        if int(result["returncode"]) != goal:
            raise ValueError(
                f"{error_message}\n\t Return code: {result['returncode']}\n\t Stderr: {result['stderr']}"
            )

    def __get_solving_script(self) -> str:
        with open(self._db_yml, "r") as f:
            data = yaml.safe_load(f)
        solver = data[self._db_key()]["solvers"][self._formalism][self._solver]["run"]
        self.logger.debug(f"Using solver script: {solver}")
        return solver

    def __before_solving_step(self) -> dict | None:
        self._allocate_temp_files()
        result = self._before_solving_step()
        return result

    def __solving_step(self) -> dict | None:
        result = self._solving_step()
        return result

    def __obtain_solution_status_step(self) -> None:
        self._solution_status = self._get_solution_status()
        self.logger.info(f"Obtained solution status: {self._solution_status} - type {type(self._solution_status)}")

    def __after_solving_step(self) -> dict | None:
        result = self._after_solving_step()
        self._free_temp_files()
        return result

    def __shutdown_run(self):
        self.logger.info(f"Pipeline status: {self._solution_status}")
        self.__wandb_run.log({"solution_status": self._solution_status})

        if (
            self._solution_status == SolutionStatus.OPTIMUM_FOUND
            or self._solution_status == SolutionStatus.SATISFIABLE
        ):
            if self._makespan is not None:
                self.logger.info(f"Project makespan: {self._makespan}")
                self.__wandb_run.log({"makespan": self._makespan})
            else:
                self.logger.warning("Makespan is not available to log.")

            if self._model is not None:
                self.logger.info(f"Solution model: {self._model}")
                self.__wandb_run.log({"model": self._model})
            else:
                self.logger.warning("Model is not available to log.")

    def __read_db(self) -> dict:
        with open(self._db_yml, "r") as f:
            data = yaml.safe_load(f)
        return data[self._db_key()]