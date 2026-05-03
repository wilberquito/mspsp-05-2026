// This script is for computing bounds for MSPSP instances.
// It can be used to compute an upper bound with a greedy heuristic, and to compute energetic precedences.


#include "parser.h"

#include "errors.h"
#include "basiccontroller.h"
#include "mspspencoding.h"
#include "solvingarguments.h"
#include "cpoptmspspencoder.h"

using namespace std;
using namespace util;

int main(int argc, char **argv)
{

	Arguments<ProgramArg> *pargs = new Arguments<ProgramArg>(

		// Program arguments
		{arguments::arg("filename", "Instance file name.")},
		1,

		// Program options
		{
            arguments::bop("U", "upper", UPPER, true,
						   "Try to compute a first UB with a greedy heuristic. Default: 1."),

			arguments::bop("", "energy", ENERGY, true,
						   "Compute energetic precedences. Default: 1."),

			arguments::bop("", "print", PRINT, false,
						   "Prints the checker result. Default: 0."),

			arguments::bop("", "print-bounds", PRINTBOUNDS, true,
						   "If true, prints the instance (after possible modifications). Default: 0."),
        },

		"Uses the heuristic to compute an upper bound for the Multi-Skill Project Scheduling Problem (MSPSP).");

	SolvingArguments * sargs = SolvingArguments::readArguments(argc,argv,pargs);

	MSPSP *instance = parser::parseMSPSP(pargs->getArgument(0));
	instance->preProcess();

	int initlb = instance->trivialLB();

	if (pargs->getBoolOption(ENERGY))
	{
		instance->computeEnergyPrecedences();
		instance->recomputeExtPrecs();
	}
	if (pargs->getBoolOption(PRINT))
	{
		cout << *instance << endl;
		exit(0);
	}

	int UB = sargs->getIntOption(UPPER_BOUND);
	int LB = sargs->getIntOption(LOWER_BOUND);
	if (LB == INT_MIN)
		LB = instance->trivialLB();
	if (pargs->getBoolOption(UPPER) && sargs->getIntOption(UPPER_BOUND) == INT_MIN)
	{
		vector<int> starts;
		vector<vector<pair<int, int>>> assignment;
		UB = instance->computePSS(starts, assignment);
		if (UB >= 0)
		{
			if (sargs->getBoolOption(PRINT_NOOPTIMAL_SOLUTIONS))
			{
				cout << "c Solution found by greedy heuristic:" << endl;
				if (sargs->getBoolOption(PRINT_CHECKS))
					BasicController::onNewBoundsProved(LB, UB);
				cout << "v ";
				instance->printSolution(cout, starts, assignment);
				cout << endl
					 << endl;
				;
			}
		}
		else
		{
			if (sargs->getBoolOption(PRINT_NOOPTIMAL_SOLUTIONS))
				cout << "c No solution found by greedy heuristic" << endl;
		}
	}
	if (UB < 0)
	{
		UB = instance->trivialUB();
	}

	if (pargs->getBoolOption(PRINTBOUNDS))
	{
		cout << "c initlb=" << initlb << " " << "lb=" << LB << " " << "ub=" << UB << endl;
	}

	return 0;
}
