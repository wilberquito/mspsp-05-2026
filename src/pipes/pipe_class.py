from src.pipes.smtapi.opb.pipe import OPB
from src.pipes.smtapi.maxsat.pipe import MAX_SAT


from src.pipes.pipe import MSPSP_Pipe
from src.entities import KnownFormalism

def get_pipe_class(str_formalism: str) -> type[MSPSP_Pipe]:
    formalism = KnownFormalism(str_formalism)
    if formalism == KnownFormalism.SAT_OPT:
        return MAX_SAT
    elif formalism == KnownFormalism.PB_OPT:
        return OPB
    else:
        raise Exception(f"Unknown formalism: {str_formalism}")

