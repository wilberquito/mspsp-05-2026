#include "mspsptimeopbencoding.h"
#include <set>
#include "util.h"
#include <cassert>

using namespace smtapi;

MSPSPTimeOPBEncoding::MSPSPTimeOPBEncoding(MSPSP *instance, SolvingArguments *sargs, Arguments<ProgramArg> *pargs) : MSPSPEncoding(instance)
{
	this->sargs = sargs;
	this->pargs = pargs;
}

MSPSPTimeOPBEncoding::~MSPSPTimeOPBEncoding()
{
}

SMTFormula *MSPSPTimeOPBEncoding::encode(int lb, int ub)
{
	int N = ins->getNActivities();
	int R = ins->getNResources();
	int L = ins->getNSkills();

	SMTFormula *f = new SMTFormula();

	// FYI: Activity continuity (4)
	// s_{i,t} -> s_{i,t+1} >= 1, 		t in [ES(i), LS(i, ub)]
	// !s_{i,t} \/ s_{i,t+1} >= 1, 		t in [ES(i), LS(i, ub)]
	for (int i = 0; i <= N + 1; i++)
	{
		boolvar s_ant;
		for (int t = ins->ES(i); t <= ins->LS(i, ub); t++)
		{
			boolvar s = f->newBoolVar("s", i, t);
			if (t > ins->ES(i))
			{
				f->addPBconstraint(!s_ant | s);
			}
			s_ant = s;
		}
	}

	// For activities with no flexibility because LS(i, ub) <= ES(i), enforce start at ES(i)
	// Normally, ES(i) < LS(i, ub), yet if the ub is forced to be less or
	// equal to the critical path of i then ES(i) == LS(i, ub)
	for (int i = 1; i <= N; i++)
	{
		int es = ins->ES(i);
		int ls = ins->LS(i, ub);
		if (ls <= es)
		{
			boolvar s = f->bvar("s", i, es);
			f->addPBconstraint(s);
		}
	}

	// FYI: Initial dummy activity (2)
	f->addPBconstraint(f->bvar("s", 0, 0));

	// FYI: minimizing the makespan of the activity N + 1
	// minimizing \sum_{t \in H} \not s_{N+1, t}
	vector<literal> goal;
	vector<literal> xs;
	// Latest dummy activity has no started before its earliest start time
	for (int t = 0; t < ins->ES(N + 1); t++)
	{
		boolvar s = f->newBoolVar("s", N + 1, t);
		xs.push_back(s);
		// f->addPBconstraint(!s);
		goal.push_back(!s);
	}

	// pb-variables of the N+1 before it can actually start at ES(N+1)
	// are forced to false
	f->addPBconstraint(std::vector<int>(xs.size(), 1), xs, "<=", 0);

	for (int t = ins->ES(N + 1); t <= ins->LS(N + 1, ub); t++)
	{
		boolvar s = f->bvar("s", N + 1, t);
		goal.push_back(!s);
	}

	// FYI: Forces that the activity N + 1 (dummy activity) has started at time ub
	f->addPBconstraint(f->bvar("s", N + 1, ub));

	f->addPBgoal(goal);

	// FYI: precedence constraints (6)
	if (pargs->getBoolOption(PBLIKE))
	{
		for (int i = 0; i <= N + 1; i++)
		{
			for (int j : ins->getSuccessors(i))
			{
				std::vector<literal> S;
				std::vector<int> Q;

				for (int t = ins->ES(i); t <= ins->LS(i, ub); t++)
				{
					S.push_back(f->bvar("s", i, t));
					Q.push_back(1);
				}

				for (int t = ins->ES(j); t <= ins->LS(j, ub); t++)
				{
					S.push_back(f->bvar("s", j, t));
					Q.push_back(-1);
				}

				// int K = ins->getDuration(i);
				int K = ins->getDuration(i) + ins->LS(i, ub) - ins->LS(j, ub);

				// Adds the PB constraint: \sum_{t} s_{i,t} - \sum_{t} s_{j,t} >= duration(i)
				f->addPBconstraint(Q, S, ">=", K);
			}
		}
	}
	else
	{
		for (int i = 0; i <= N + 1; i++)
		{
			for (int j : ins->getSuccessors(i))
			{
				for (int t = ins->ES(i); t <= ins->LS(i, ub); t++)
				{
					// If both activities overlaps in time...
					if (t + ins->getDuration(i) >= ins->ES(j))
					{
						// Confirmaiton of overlap implies precedence
						if (t + ins->getDuration(i) <= ins->LS(j, ub))
							// If j has started t + duration(i), then i has started at t
							f->addPBconstraint(!f->bvar("s", j, t + ins->getDuration(i)) | f->bvar("s", i, t));
						else
							// Enforces that activity i has started at time t
							f->addPBconstraint(f->bvar("s", i, t));
					}
				}
			}
		}
	}

	// FYI: Resource assigned to an activity (10)
	// ar_{i,k} -> \sum_{L_l \in L(A_i)} ars_{i,k,l} >= 1, 		\forall A_i \in A, \forall k \in R

	// FYI: Resources are unary. (16), i.e., a resource can only provide at most one skill per activity
	// \sum_{L_l \in L(A_i)} ars_{i,k,l} <= 1, 		\forall A_i \in A, \forall k \in R

	for (int i = 1; i <= N; i++)
	{
		for (int k = 0; k < R; k++)
		{
			if (ins->usefulToActivity(k, i))
			{
				boolvar ar = f->newBoolVar("act_res", i, k);
				vector<literal> v;
				for (int l = 0; l < L; l++)
				{
					if (ins->hasSkill(k, l) && ins->demandsSkill(i, l))
					{
						boolvar ars = f->newBoolVar("act_res_skill", i, k, l);
						v.push_back(ars);
					}
				}
				f->addPBOrReification(ar, v);
				std::vector<int> q = std::vector<int>(v.size(), 1);
				f->addPBconstraint(q, v, "<=", 1);
			}
		}
	}

	// FYI: Skill coverage requirement (14)
	// \sum_{k \in R} ars_{i,k,l} >= b_i,l, 		\forall A_i \in A, \forall L_l \in L(A_i)
	for (int i = 1; i <= N; i++)
	{
		for (int l = 0; l < L; l++)
		{
			if (ins->demandsSkill(i, l))
			{
				vector<literal> v;
				for (int k = 0; k < R; k++)
				{
					if (ins->hasSkill(k, l))
						v.push_back(f->bvar("act_res_skill", i, k, l));
				}
				std::vector<int> q = std::vector<int>(v.size(), 1);
				int K = ins->getDemand(i, l);
				if (pargs->getBoolOption(FULL))
					f->addPBconstraint(q, v, "=", K);
				else
					f->addPBconstraint(q, v, ">=", K);
			}
		}
	}

	// FYI: Activity running at a particular time (4)
	// x_{i,t} <-> s_{i,t} + \not s_{i,t+p_i} >= 2, 		t \in [ES(i), LS(i, ub)]
	for (int i = 1; i <= N; i++)
	{
		for (int t = ins->ES(i); t < ins->LC(i, ub); t++)
		{
			boolvar x = f->newBoolVar("x", i, t);
			std::vector<literal> v;
			if (t <= ins->LS(i, ub))
			{
				v.push_back(f->bvar("s", i, t));
			}
			if (t - ins->getDuration(i) >= ins->ES(i))
			{
				boolvar s_i_t1 = f->bvar("s", i, t - ins->getDuration(i));
				v.push_back(!s_i_t1);
			}
			if (v.empty())
			{
				f->addPBconstraint(x);
			}
			else
			{
				f->addPBAndReification(x, v);
			}
		}
	}

	// FYI: Resource assigned to an activity at a particular time (12)
	// art_{i,k,t} <-> x_{i,t} + ars_{i,k,l} >= 2, 		t \in [ES(i), LS(i, ub)], \forall L_l \in L(A_i)
	//
	// FYI: Resource capacity per time (18)
	// \sum_{i \in A} art_{i,k,t} <= 1 for each k \in R, t \in [0, ub]
	for (int k = 0; k < R; k++)
	{
		for (int t = 0; t <= ub; t++)
		{
			vector<literal> v;
			for (int i = 1; i <= N; i++)
			{
				if (ins->ES(i) <= t && t < ins->LC(i, ub) && ins->usefulToActivity(k, i))
				{
					boolvar art = f->newBoolVar("act_res_time", i, k, t);
					std::vector<literal> v2;
					v2.push_back(f->bvar("x", i, t));
					v2.push_back(f->bvar("act_res", i, k));

					v.push_back(art);
					f->addPBAndReification(art, v2);
				}
			}
			if (!v.empty())
			{
				std::vector<int> q = std::vector<int>(v.size(), 1);
				f->addPBconstraint(q, v, "<=", 1);
			}
		}
	}

	// Implied 1: For each skill l, the demand of l by activities running at a particular time
	//	is not greater than the number of resources mastering the skill
	if (pargs->getBoolOption(IMPLIED1))
	{
		for (int l = 0; l < L; l++)
		{
			for (int t = 0; t < ub; t++)
			{
				vector<literal> X;
				vector<int> Q;

				for (int i = 1; i <= N; i++)
				{
					int ESi = ins->ES(i);
					int LCi = ins->LC(i, ub);
					if (t >= ESi && t < LCi && ins->demandsSkill(i, l))
					{
						X.push_back(f->bvar("x", i, t));
						Q.push_back(ins->getDemand(i, l));
					}
				}

				if (!X.empty())
				{
					f->addPBconstraint(Q, X, "<=", ins->getNResourcesMastering(l));
				}
			}
		}
	}

	// Implied 2: the number of skill demands of activities running at a particular time
	//	is not greater than the number of resources
	if (pargs->getBoolOption(IMPLIED2))
	{
		for (int t = 0; t < ub; t++)
		{
			vector<literal> X;
			vector<int> Q;

			for (int i = 1; i <= N; i++)
			{
				int ESi = ins->ES(i);
				int LCi = ins->LC(i, ub);
				if (t >= ESi && t < LCi)
				{
					X.push_back(f->bvar("x", i, t));
					Q.push_back(ins->getTotalDemand(i));
				}
			}

			if (!X.empty())
			{
				f->addPBconstraint(Q, X, "<=", R);
			}
		}
	}

	// Implied 3: the number of activities running at a particular time
	//	is not greater than the number of resources available to cover
	//	any subset of skills
	//  There is a dominance precomputation to avoid the introduction
	//  of dominated constraints
	if (pargs->getBoolOption(IMPLIED3))
	{
		int nImplied = 0;

		// Compute dominances
		int nCombinations = 1 << ins->getNSkills();
		// vector<bool> dominated(nCombinations,false);

		// For each combination of skills, compute the number of resources
		// that can cover at least one of the skills in the combination.
		vector<int> capacity(nCombinations, 0);
		for (int combination = 1; combination < nCombinations; combination++)
		{
			for (int k = 0; k < R; k++)
			{
				for (int l = 0; l < ins->getNSkills(); l++)
					if (util::nthBit(combination, l) && ins->hasSkill(k, l))
					{
						capacity[combination]++;
						break;
					}
			}
		}

		for (int t = 0; t < ub; t++)
		{
			// Computes the required skills at time instant t, \forall A_i \in A
			vector<bool> required(ins->getNSkills(), false);

			for (int i = 1; i <= N; i++)
			{
				int ESi = ins->ES(i);
				int LCi = ins->LC(i, ub);
				if (t >= ESi && t < LCi)
				{
					for (int l = 0; l < ins->getNSkills(); l++)
						if (ins->demandsSkill(i, l))
							required[l] = true;
				}
			}

			// At each time instant, only consider the subsets that contain skills required
			// by some of the activities that can be running at this particular time
			// For each non-dominated combination of skills
			//
			// The subsets are represented numerically from 1..nCombinations. Accessing to
			// the bits activated in base 2 you get each of the sets of the power set.
			for (int combination = 1; combination < nCombinations; combination++)
			{ // Has to be read as a binary number
				bool allrequired = true;
				for (int l = 0; l < ins->getNSkills(); l++)
					if (util::nthBit(combination, l) && !required[l])
					{
						allrequired = false;
						break;
					}

				// If every skill is required and the combination is not dominated
				if (allrequired && !ins->isDominated(combination))
				{
					vector<literal> X;
					vector<int> Q;
					for (int i = 1; i <= N; i++)
					{
						int ESi = ins->ES(i);
						int LCi = ins->LC(i, ub);
						if (t >= ESi && t < LCi)
						{
							int demand = 0;
							for (int l = 0; l < ins->getNSkills(); l++)
								if (util::nthBit(combination, l))
									demand += ins->getDemand(i, l);

							if (demand != 0)
							{
								X.push_back(f->bvar("x", i, t));
								Q.push_back(demand);
							}
						}
					}
					if (!X.empty())
					{
						nImplied++;
						f->addPBconstraint(Q, X, "<=", capacity[combination]);
					}
				}
			}
		}
		// cout << "c nImpliedAsserted is " << nImplied << "/" << (ub * (1<<ins->getNSkills())) << endl;
	}

	//	// Symetry breaking: set an order on the use of identical resources
	//	if (pargs->getBoolOption(SYMBREAK))
	//		for (int t = 0; t < ub; t++)
	//		{
	//			// Introduce variable "res_time"
	//			for (int k = 0; k < R; k++)
	//			{
	//				boolvar v = f->newBoolVar("res_time", k, t);
	//				clause c = !v;
	//				for (int i = 1; i <= N; i++)
	//				{
	//					if (ins->usefulToActivity(k, i) && ins->ES(i) <= t && t < ins->LC(i, ub))
	//					{
	//						f->addPBconstraint(!f->bvar("act_res_time", i, k, t) | v);
	//						c |= f->bvar("act_res_time", i, k, t);
	//					}
	//				}
	//				f->addPBconstraint(c);
	//			}
	//
	//			for (int type = 0; type < ins->getNResourceTypes(); type++)
	//			{
	//				const vector<int> &res_type = ins->getResourcesOfType(type);
	//				int nOfType = res_type.size();
	//				for (int ri = 0; ri < nOfType - 1; ri++)
	//				{
	//					for (int i = 1; i <= N; i++)
	//						if (ins->usefulToActivity(res_type[ri], i) && ins->ES(i) <= t && t <= ins->LS(i, ub))
	//						{
	//							for (int rj = ri + 1; rj < nOfType; rj++)
	//							{
	//								// If resource  'res_type[ri]'  starts to work at an activity at time 't',
	//								//  then every resource 'res_type[rj]' of the same type s.t  'rj' precedes 'ri' has to be occupied
	//								f->addPBconstraint(
	//									(t - 1 < ins->ES(i) ? f->falseVar() : f->bvar("act_res_time", i, res_type[ri], t - 1)) |
	//									!f->bvar("act_res_time", i, res_type[ri], t) |
	//									f->bvar("res_time", res_type[rj], t));
	//							}
	//						}
	//				}
	//			}
	//		}
	//

	return f;
}

void MSPSPTimeOPBEncoding::setModel(const EncodedFormula &ef, int lb, int ub, const vector<bool> &bmodel, const vector<int> &imodel)
{
	int N = ins->getNActivities();
	int R = ins->getNResources();
	int L = ins->getNSkills();

	this->starts = vector<int>(N + 2);
	this->assignment.clear();
	this->assignment.resize(N + 2);

	for (int i = 0; i <= N + 1; i++)
	{
		this->starts[i] = SMTFormula::getIValue(ef.f->ivar("S", i), imodel);
		for (int l = 0; l < L; l++)
		{
			int demand = ins->getDemand(i, l);
			for (int k = 0; k < R; k++)
			{
				if (ins->hasSkill(k, l) && ins->demandsSkill(i, l))
				{
					if (SMTFormula::getBValue(ef.f->bvar("act_res_skill", i, k, l), bmodel))
					{
						if (demand > 0)
						{
							assignment[i].push_back(pair<int, int>(k, l));
							demand--;
						}
					}
				}
			}
		}
	}
}

bool MSPSPTimeOPBEncoding::narrowBounds(const EncodedFormula &ef, int lastLB, int lastUB, int lb, int ub)
{
	int N = ins->getNActivities();
	if (ub <= lastUB)
	{
		ef.f->addPBconstraint(ef.f->ivar("S", N + 1) <= ub);
		ef.f->addPBconstraint(ef.f->ivar("S", N + 1) >= lb);
		return true;
	}
	else
		return false;
}

void MSPSPTimeOPBEncoding::assumeBounds(const EncodedFormula &ef, int lb, int ub, vector<literal> &assumptions)
{
	int N = ins->getNActivities();
	assumptions.push_back(ef.f->ivar("S", N + 1) <= ub);
	assumptions.push_back(ef.f->ivar("S", N + 1) >= lb);
}

MSPSPModel MSPSPTimeOPBEncoding::makeModel(const EncodedFormula &ef, const istream &bmodel) const
{
	vector<bool> model;
	readModel(const_cast<istream &>(bmodel), ' ', model);
	return MSPSPEncoding::_makeModel(ef, model);
}

// returns true if found (and consumes the char), false otherwise
bool MSPSPTimeOPBEncoding::findChar(std::istream &model_str, char c) const
{
	char aux;
	while (model_str.get(aux))
	{ // stop if read fails (EOF or error)
		if (aux == c)
			return true;
	}
	return false;
}

void MSPSPTimeOPBEncoding::readModel(istream &model_str, char c, vector<bool> &model) const
{
	findChar(model_str, c); // Consume until marker

	if (!model_str || model_str.eof())
	{
		cerr << "Error: no model found in the input stream." << endl;
		exit(BADFILE_ERROR);
	}

	string token;
	while (model_str >> token)
	{
		if (token[0] == '-')
			model.push_back(false);
		else
			model.push_back(true);
	}
}
