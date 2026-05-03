from enum import Enum

type Script = str

class KnownSolver(Enum):
    CASHWMAXSAT = "cashwmaxsat"
    EVALMAXSAT = "evalmaxsat"
    EXACT = "exact"
    MAXCDCL = "maxcdcl"
    PACOSE = "pacose"
    SCIP = "scip"
    UWRMAXSAT = "uwrmaxsat"
    ROUNDINGSAT = "roundingsat"
    SAT4JPB = "sat4jpb"
    EXACTPB = "exactpb"

class KnownFormalism(Enum):
    SAT = "sat"
    SAT_OPT = "maxsat"
    PB = "pb"
    PB_OPT = "opb"
    CP = "cp"
    CP_OPT = "ocp"

class KnownFamily(Enum):
    SET_1A = "set-1a"
    SET_1B = "set-1b"
    SET_2A = "set-2a"
    SET_2B = "set-2b"
    SET_2C = "set-2c"
    SET_3A = "set-3a"
    SET_3B = "set-3b"