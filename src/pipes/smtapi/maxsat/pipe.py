from src.pipes.pipe import SolutionStatus

from src.pipes.smtapi.pipe import SMT_API_Pipe
from src.timedcommand import cmd_with_linux_time


class MAX_SAT(SMT_API_Pipe):

    @staticmethod
    def register_args(parser):
        SMT_API_Pipe.register_args(parser)

    def __init__(self, args):
        super().__init__(args)

    def _encoding_cmd(self) -> list:
        return [
            self._encoder_script,
            self._mspsp_instance,
            self._i1,
            self._i2,
            self._i3,
            self._i4,
        ]

    def _model_checker_cmd(self) -> list:
        return [
            self._model_checker_script,
            self._mspsp_instance,
            self._model_file_out.name,
            self._validated_model_file_out.name,
            self._i1,
            self._i2,
            self._i3,
            self._i4,
        ]

    def _after_solving_step(self) -> dict | None:
        if self._solution_status is None:
            raise RuntimeError("Solution status is not set. Cannot proceed to after solving step.")

        if (
            self._solution_status == SolutionStatus.OPTIMUM_FOUND
            or self._solution_status == SolutionStatus.SATISFIABLE
        ):
            cmd = [self._substract_last_optim_script, self._solver_file_out.name]
            result = cmd_with_linux_time(cmd, text=True)
            if result["returncode"] != 0:
                self.logger.error(
                    f"Error running substract_last_optim_script: {result['stderr']}"
                )
                raise RuntimeError(
                    f"Substract last optim script failed with return code {result['returncode']}"
                )
            makespan = result["stdout"].strip()
            self._makespan = int(makespan)
            self._model = self._get_model()

        # This pipeline is ment to find the optimun in the provided timeout (s)
        if self._solution_status != SolutionStatus.OPTIMUM_FOUND:
            self.logger.warning(
                f"Optimum not found with timeout {self._solving_timeout_s}"
            )
