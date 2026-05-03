from abc import abstractmethod
import subprocess
import tempfile
from typing import Any

from src.pipes.pipe import MSPSP_Pipe, SolutionStatus
from src.timedcommand import cmd_with_linux_time


class SMT_API_Pipe(MSPSP_Pipe):
    __status_map = {
        "s OPTIMUM FOUND": SolutionStatus.OPTIMUM_FOUND,
        "s UNSATISFIABLE": SolutionStatus.UNSATISFIABLE,
        "s SATISFIABLE": SolutionStatus.SATISFIABLE,
        "s UNKNOWN": SolutionStatus.UNKNOWN,
    }

    def __init__(self, args):
        super().__init__(args)

        # Implied flags self._i1 = args.implied_1
        self._i1 = args.implied_1
        self._i2 = args.implied_2
        self._i3 = args.implied_3
        self._i4 = args.implied_4

        # Scripts
        self._encoder_script = self.__get_encoder_script()
        self._model_checker_script = self.__get_model_checker_script()
        self._substract_model_script = self.__get_substract_model_script()
        self._substract_last_optim_script = self.__get_substract_last_optim_script()

    @staticmethod
    def register_args(parser):
        parser.add_argument(
            "--implied-1",
            type=int,
            default=0,
            choices=[0, 1],
            help="Use implied constraint type 1",
        )
        parser.add_argument(
            "--implied-2",
            type=int,
            default=0,
            choices=[0, 1],
            help="Use implied constraint type 2",
        )
        parser.add_argument(
            "--implied-3",
            type=int,
            default=0,
            choices=[0, 1],
            help="Use implied constraint type 3",
        )
        parser.add_argument(
            "--implied-4",
            type=int,
            default=0,
            choices=[0, 1],
            help="Use implied constraint type 4",
        )

    def config(self) -> dict:
        base_config = super().config()
        local_config = {
            "implied_1": self._i1,
            "implied_2": self._i2,
            "implied_3": self._i3,
            "implied_4": self._i4,
        }
        return { **base_config, **local_config }

    def inline_config(self) -> str:
        return f"f={self._formalism}__s={self._solver}__i={self.__implied_seq()}"

    @abstractmethod
    def _after_solving_step(self) -> dict | None:
        pass

    @abstractmethod
    def _encoding_cmd(self) -> list:
        pass

    @abstractmethod
    def _model_checker_cmd(self) -> list:
        pass

    def _db_key(self) -> str:
        return "smtapi"

    def _log_configuration_step(self) -> None:
        self.logger.info(f"Instance: {self._mspsp_instance}")
        self.logger.info(f"Implied sequence: {self.__implied_seq()}")
        self.logger.info(f"Using encoder script: {self._encoder_script}")
        self.logger.info(f"Using solving script: {self._solving_script}")
        self.logger.info(f"Solving timeout (s): {self._solving_timeout_s}")

    def _allocate_temp_files(self) -> None:
        self._encoder_file_out = tempfile.NamedTemporaryFile(mode="w+", suffix=".encoder.out", delete=False)
        self._solver_file_out = tempfile.NamedTemporaryFile(mode="w+", suffix=".solver.out", delete=False)
        self._model_file_out = tempfile.NamedTemporaryFile(mode="w+", suffix=".model.out", delete=False)
        self._validated_model_file_out = tempfile.NamedTemporaryFile(mode="w+", suffix=".solution.out", delete=False)

    def _free_temp_files(self) -> None:
        self._encoder_file_out.close()
        self._solver_file_out.close()
        self._model_file_out.close()
        self._validated_model_file_out.close()

    def _before_solving_step(self) -> dict | None:
        result = self.__encoding()
        self._verify_result(result, "Encoder failed")
        metrics = self._metrics(result, "encoder")
        return metrics

    def __encoding(self) -> dict:
        cmd = self._encoding_cmd()
        return cmd_with_linux_time(cmd, stdout=self._encoder_file_out, text=True)

    def _solving_step(self) -> dict:
        cmd: list[Any] = [
            self._solving_script,
            self._encoder_file_out.name,
        ]
        result = cmd_with_linux_time(cmd, stdout=self._solver_file_out, text=True, timeout=self._solving_timeout_s)
        metrics = self._metrics(result, "solver")
        return metrics

    def _get_solution_status(self) -> SolutionStatus:
        sol_label = "^s"
        command = f"grep {sol_label} {self._solver_file_out.name}"
        self.logger.debug(f"💲 {command}")
        completed = subprocess.run(
            command, capture_output=True, text=True, check=False, shell=True
        )
        status = SolutionStatus.UNKNOWN
        if completed.returncode != 0:
            self.logger.warning(
                f"Failed to extract solution status from solver output.\nCommand: {command}\nReturn code: {completed.returncode}\nStderr: {completed.stderr}"
            )
        else:
            output = completed.stdout.strip()
            self.logger.info(f"Extracted solution status line: '{output}'")
            status = self.__status_map.get(output, SolutionStatus.UNKNOWN)

        return status

    def _get_model(self) -> str | None:
        try:
            self.__substract_model()
            self.__validate_model()
            with open(self._validated_model_file_out.name, "r") as f:
                model = f.read()
            return model
        except Exception as e:
            self.logger.warning(f"Failed to get the model: {e}")

    def __substract_model(self) -> None:
        # Subsctract the model from the solver output
        cmd = [self._substract_model_script, self._solver_file_out.name]
        result = cmd_with_linux_time(cmd, stdout=self._model_file_out, text=True)
        self._verify_result(result, "Model substraction failed")

    def __validate_model(self) -> dict:
        # Verify that the model is correct and get the mapped solution
        cmd = self._model_checker_cmd()
        result = cmd_with_linux_time(cmd, text=True)
        self._verify_result(result, "Model checker failed")
        metrics = self._metrics(result, "model_checker")
        return metrics

    def __implied_seq(self):
        implieds = [self._i1, self._i2, self._i3, self._i4]
        implied_list = [
            f"i{i}={int(implied)}" for i, implied in enumerate(implieds, start=1)
        ]
        return ",".join(implied_list)

    def __get_encoder_script(self):
        return self._db_data["encoders"][self._formalism]["run"]

    def __get_model_checker_script(self):
        return self._db_data["model_checkers"][self._formalism]["run"]

    def __get_substract_model_script(self):
        return self._db_data["substract_model"]["run"]

    def __get_substract_last_optim_script(self):
        return self._db_data["substract_last_optim"]["run"]
