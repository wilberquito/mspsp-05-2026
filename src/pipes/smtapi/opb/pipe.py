from src.pipes.pipe import SolutionStatus
from src.pipes.smtapi.pipe import SMT_API_Pipe, cmd_with_linux_time


class OPB(SMT_API_Pipe):

    @staticmethod
    def register_args(parser):
        SMT_API_Pipe.register_args(parser)
        parser.add_argument(
            "--pb-like",
            type=int,
            default=0,
            choices=[0, 1],
            help="Encoding the precedences as PB constraints instead of SAT clauses",
        )

    def __init__(self, args):
        super().__init__(args)

        self.__pb_like = args.pb_like
        self.__substract_fn_obj_size_script = self.__get_substract_fn_size_script()


    def config(self) -> dict:
        parent_config = super().config()
        local_config = {
            "pb_like": self.__pb_like,
        }
        return {**parent_config, **local_config}

    def inline_config(self) -> str:
        parent_inline_config = super().inline_config()
        return f"{parent_inline_config}__pb_like={self.__pb_like}"

    def _encoding_cmd(self) -> list:
        return [
            self._encoder_script,
            self._mspsp_instance,
            self._i1,
            self._i2,
            self._i3,
            self._i4,
            self.__pb_like
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
            self.__pb_like
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
            # Local optim by the solver (optim <= 0)
            optim = result["stdout"].strip()
            optim = int(optim)

            cmd = [self.__substract_fn_obj_size_script, self._encoder_file_out.name]
            result = cmd_with_linux_time(cmd, text=True)
            if result["returncode"] != 0:
                self.logger.error(
                    f"Error running substract_fn_obj_size_script: {result['stderr']}"
                )
                raise RuntimeError(
                    f"Substract fn obj size script failed with return code {result['returncode']}"
                )

            obj_size = result["stdout"].strip()
            obj_size = int(obj_size)

            self._makespan = obj_size + optim
            self._model = self._get_model()

        # This pipeline is ment to find the optimun in the provided timeout (s)
        if self._solution_status != SolutionStatus.OPTIMUM_FOUND:
            self.logger.warning(
                f"Optimum not found with timeout {self._solving_timeout_s}"
            )

    def __get_substract_fn_size_script(self) -> str:
        return self._db_data["substract_fn_obj_size"]["run"]
