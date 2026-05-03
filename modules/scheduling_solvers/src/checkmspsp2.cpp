#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include "mspsp.h"
#include "errors.h"
#include "parser.h"
#include "arguments.h"
#include "solvingarguments.h"
#include "mspspencoding.h"
#include "mspsptimemaxsatencoding.h"
#include "mspsptimesatencoding.h"
#include "mspsptimeopbencoding.h"
#include "mspsptimepbencoding.h"
#include "basiccontroller.h"

using namespace std;
using namespace arguments;


bool error;
stringstream errstream;

bool warning;
stringstream warnstream;

void checkSolution(MSPSP * ins, const vector<int> & starts, const vector<vector<pair<int,int> > > & assignments)
{
	int N = ins->getNActivities();
	int R = ins->getNResources();
	int L = ins->getNSkills();

	error = false;

	int makespan = starts[N + 1];
	for(int i = 0; i <= N + 1; i++)
	{
		if(starts[i] + ins->getDuration(i) > makespan)
		{
			errstream << "Error: the closing activity is not the last one to finish. Makespan is " << makespan << " and activity " << i << " ends at " << (starts[i]+ins->getDuration(i)) << endl;
			error = true;
		}
	}


	errstream << "Resources:" << endl;
	errstream << "------------------------------------------------------------" << endl;

	vector<vector<int>> activity_skill(N + 2, vector<int>(L, 0));
	vector<vector<bool>> activity_resource(N + 2, vector<bool>(R, false));

	for(int i = 0; i < N + 2; i++)
	{
		for(int l = 0; l < L; l++)
		{
			activity_skill[i][l] = ins->getDemand(i,l);
		}
	}

	for(int i = 0; i < N + 2; i++)
	{
		for(const pair<int,int> & p : assignments[i])
		{
			int k = p.first;
			int l = p.second;
			if(!ins->hasSkill(k, l))
			{
				errstream << "Error: resource " << k <<
					" cannot perform skill " << l << " in activity " << (i+1) <<
					" since it does not master it." << endl;
				error = true;
			}

			if(activity_resource[i][k])
			{
				errstream << "Error: resource " << k <<
					" asked to contribute more than 1 to activity " << (i+1) << endl;
				error = true;
			}

			activity_resource[i][k] = true;
			activity_skill[i][l]--;
		}
	}

	for(int i = 0; i < N + 2; i++)
	{
		for(int l = 0; l < L; l++)
		{
			if(activity_skill[i][l] > 0)
			{
				errstream << "Error: activity " << i
					<< " has not enough resources covering skill " << l
					<< ". Required " << ins->getDemand(i,l)
					<< ", assigned " << (ins->getDemand(i,l) - activity_skill[i][l]) << endl;
				error = true;
			}
			else if(activity_skill[i][l] < 0)
			{
				warnstream << "Warning: activity " << i
					<< " has extra resources covering skill " << l
					<< ". Required " << ins->getDemand(i,l)
					<< ", assigned " << (ins->getDemand(i,l) - activity_skill[i][l]) << endl;
				warning = true;
			}
		}
	 }


	for(int i = 0; i < ins->getNActivities()+2; i++)
	{
		for(int j = i+1; j < ins->getNActivities()+2; j++)
		{
			if(i!=j)
			{
				for(int k = 0; k < ins->getNResources(); k++) {
					if(activity_resource[i][k] && activity_resource[j][k])
					{
						if((starts[i] <= starts[j] && starts[j] < starts[i] + ins->getDuration(i))
								||(starts[j] <= starts[i] && starts[i] < starts[j] + ins->getDuration(j)))
						{
							errstream << "Error: activities " << i << " and " << j
								<< " require resource " << k << " at the same time" << endl;
							error = true;
						}
					}
				}
			}
		}
	}


	errstream << endl;

	errstream << "Precedences:" << endl;
	errstream << "------------------------------------------------------------" << endl ;
	for(int i = 0; i < N + 2; i++)
	{
		for(int suc : ins->getSuccessors(i))
		{
			if(starts[suc] < starts[i] + ins->getDuration(i))
			{
				errstream << "Error: precedence " << i << " ---> " << suc << " not respected" << endl;
				error = true;
			}
		}
	}
}



int main(int argc, char **argv) {
	Arguments<ProgramArg> * pargs = new Arguments<ProgramArg>(
		//Program arguments
		{
		arguments::arg("ins","Instance file path."),
		arguments::arg("model","Solver model."),
		arguments::arg("solution","Solution file path."),
		},
		3,

		//Program options
		{
			arguments::bop("", "implied1", IMPLIED1, false,
						   "Add the implied constrant: the number of activities requiring skill 'l' at a particular time is not greater than the number of resources that master 'l'. Default: 0."),

			arguments::bop("", "implied2", IMPLIED2, false,
						   "Add the implied constrant: the number of activities running at a particular time is not greater than the number of resources. Default: 0."),

			arguments::bop("", "implied3", IMPLIED3, false,
						   "Add the implied constrant: for any combination of skills (dominance filtering applies), the number of activities running at a particular time is not greater than the number of resources that master those skills. Default: 1."),

			arguments::bop("", "implied4", IMPLIED4, false,
						   "Add the implied constraint: a running activity at some time point implied that the number of assigned resources is greater or equal to the demand of the activity. Default: 0."),

			arguments::bop("", "pblike", PBLIKE, false,
						   "Forces the use of pb-like encodings instead of sat-like encodings when possible. Default: 0."),

			arguments::sop("E", "encoding", ENCODING, "timemaxsat", {"timemaxsat", "timeopb", "timesat", "timepb"},
						   "Encoding to use. Default: timemaxsat."),

			arguments::bop("U", "upper", UPPER, true,
						   "Try to compute a first UB with a greedy heuristic. Default: 1."),

			arguments::bop("", "energy", ENERGY, true,
						   "Compute energetic precedences. Default: 1."),

			arguments::bop("", "print", PRINT, true,
						   "Prints the checker result. Default: 1."),

			arguments::bop("F", "full", FULL, false,
						   "If true requires that the number of suplied skills is exactly the demand. Else, it must be at least the demand (still correct solutions in output). Default: 0."),
		},

		"Check if a solution of the Multi-Skill Project Scheduling Problem (MSPSP) is correct."
	);

	SolvingArguments * sargs = SolvingArguments::readArguments(argc,argv,pargs);

	MSPSP * ins = parser::parseMSPSP(pargs->getArgument(0));
	ins->preProcess();

	if (pargs->getBoolOption(ENERGY))
	{
		ins->computeEnergyPrecedences();
		ins->recomputeExtPrecs();
	}

	bool sat = true;
	int UB = sargs->getIntOption(UPPER_BOUND);
	int LB = sargs->getIntOption(LOWER_BOUND);
	if (LB == INT_MIN)
		LB = ins->trivialLB();
	if (pargs->getBoolOption(UPPER) && sargs->getIntOption(UPPER_BOUND) == INT_MIN)
	{
		vector<int> starts;
		vector<vector<pair<int, int>>> assignment;
		UB = ins->computePSS(starts, assignment);
		if (UB >= 0)
		{
			if (sargs->getBoolOption(PRINT_NOOPTIMAL_SOLUTIONS))
			{
				cout << "c Solution found by greeey heuristic:" << endl;
				if (sargs->getBoolOption(PRINT_CHECKS))
					BasicController::onNewBoundsProved(LB, UB);
				cout << "v ";
				ins->printSolution(cout, starts, assignment);
				cout << endl << endl;
			}
		}
		else
		{
			if (sargs->getBoolOption(PRINT_NOOPTIMAL_SOLUTIONS))
				cout << "c No solution found by greeey heuristic" << endl;
		}
	}
	if (UB < 0)
	{
		UB = ins->trivialUB();
		sat = false;
	}

	ifstream model_file;
	if(pargs->getNArguments() >= 2)
	{
		model_file.open(pargs->getArgument(1).c_str());
		if (!model_file.is_open())
		{
			cerr << "Could not open model file " << pargs->getArgument(1) << endl;
			exit(BADFILE_ERROR);
		}
	}

	istream & model_str = pargs->getNArguments() >= 2 ? model_file : cin;

	MSPSPEncoding *encoder;
	if (pargs->getStringOption(ENCODING) == "timemaxsat")
	{
		encoder = new MSPSPTimeMaxSATEncoding(ins, sargs, pargs);
	}
	else if (pargs->getStringOption(ENCODING) == "timesat")
	{
		encoder = new MSPSPTimeSATEncoding(ins, sargs, pargs);
	}
	else if (pargs->getStringOption(ENCODING) == "timeopb")
	{
		encoder = new MSPSPTimeOPBEncoding(ins, sargs, pargs);
	}
	else if (pargs->getStringOption(ENCODING) == "timepb")
	{
		encoder = new MSPSPTimePBEncoding(ins, sargs, pargs);
	}
	else {
		cerr << "Unknown encoder " << pargs->getStringOption(ENCODING) << endl;
		exit(BADARGUMENTS_ERROR);
	}

	SMTFormula *f = encoder->encode(LB, UB);
	EncodedFormula ef(f, LB, UB);
	MSPSPModel model = encoder->makeModel(ef, model_str);

	ofstream model_out;
	model_out.open(pargs->getArgument(2).c_str());

	model_out << "v ";
	encoder->printSolution(model_out, model);

	model_out.close();

	checkSolution(ins, model.getStarts(), model.getAssignment());

	if (error)
		std::cerr << pargs->getArgument(0)<< ":\t" << "ERROR" << std::endl;
	else
		std::cout << pargs->getArgument(0) << ":\tOK" << std::endl;

	if (warning && pargs->getBoolOption(PRINT))
	{
		std::cout << "============================================================" << std::endl;
		std::cout << warnstream.str();
		std::cout << "============================================================" << std::endl;
	}

	if(error && pargs->getBoolOption(PRINT))
	{
		std::cerr << "============================================================" << std::endl;
		std::cerr << errstream.str();
		std::cerr << "============================================================" << std::endl;
    }

	if (model_file.is_open())
		model_file.close();

	if (model_out.is_open())
		model_out.close();

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}

