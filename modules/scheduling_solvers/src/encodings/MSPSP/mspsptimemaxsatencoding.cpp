#include "mspsptimemaxsatencoding.h"
#include <set>
#include "util.h"
#include <cassert>

using namespace smtapi;

MSPSPTimeMaxSATEncoding::MSPSPTimeMaxSATEncoding(MSPSP *instance, SolvingArguments *sargs, Arguments<ProgramArg> *pargs) : MSPSPEncoding(instance)
{
	this->sargs = sargs;
	this->pargs = pargs;
}

MSPSPTimeMaxSATEncoding::~MSPSPTimeMaxSATEncoding()
{
}

SMTFormula *MSPSPTimeMaxSATEncoding::encode(int lb, int ub)
{
	int N = ins->getNActivities();
	int R = ins->getNResources();
	int L = ins->getNSkills();

	SMTFormula *f = new SMTFormula();

	// Activity continuity (3)

	// Once an activity has started at time t + 1, it must have started at time t
	for (int i = 0; i <= N + 1; i++)
	{
		boolvar s_ant;
		for (int t = ins->ES(i); t <= ins->LS(i, ub); t++)
		{
			boolvar s = f->newBoolVar("s", i, t);
			if (t > ins->ES(i))
				f->addClause(!s_ant | s);
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
			f->addClause(s);
		}
	}

	// Initial dummy activity (1)

	// Activity 0  (dummy activity) starts at time 0
	f->addClause(f->bvar("s", 0, 0));

	// Objective function
	for (int t = 0; t < ins->ES(N + 1); t++)
	{
		boolvar s = f->newBoolVar("s", N + 1, t);
		// Per each time 't' before the earliest start of the last activity,
		// we add a clause meaning that the last activity cannot start at time 't'
		f->addClause(!f->bvar("s", N + 1, t));
		// We also add a soft clause, so the solver will try to minimize the makespan
		// these soft clauses will not be satisfied, but they will be used to minimize the makespan
		f->addSoftClause(f->bvar("s", N + 1, t));
	}

	// Activity N + 1 (dummy activity) set bounds on makespan
	// Forces the latest dummy activity started at 'ub'
	f->addClause(f->bvar("s", N + 1, ub));

	// For the last activity, we add soft clauses. So the solver will try to
	// minimize the makespan of the last activity.
	for (int t = ins->ES(N + 1); t <= ins->LS(N + 1, ub); t++)
		f->addSoftClause(f->bvar("s", N + 1, t));

	// Precedence constraints (5)

	// Per each pair of activities (i,j) such that i precedes j,
	// then if i starts at time t, j has to start at least at time t + duration(i)
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
						f->addClause(!f->bvar("s", j, t + ins->getDuration(i)) | f->bvar("s", i, t));
					else
						// Enforces that activity i has started at time t
						f->addClause(f->bvar("s", i, t));
				}
			}
		}
	}

	// Resource assignment to activity (9)
	// At most one skill 'l' per resource 'k' per activity (15)

	// This codifies that if and only if a resource k is used in an activity i, then
	// that resource has one of the skill required by the activity i.
	// It also codifies that a resource k can only be used in the activity i at most once
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
						f->addClause(!ars | ar); // Channelling act_res act_res_skill
					}
				}

				// Each resource spends at most one skill in each activity
				// TODO: check if this is correct (it takes into account the activity resource)
				f->addAMO(v, sargs->getAMOEncoding());

				// Channelling act_res act_res_skill
				v.push_back(!ar);
				f->addClause(v);
			}
		}
	}

	// Skill coverage requirement (13)

	// Ensures that each activity i has enough resources to cover the skills required
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
				if (pargs->getBoolOption(FULL))
					f->addEK(v, ins->getDemand(i, l));
				else
					f->addALK(v, ins->getDemand(i, l), sargs->getCardinalityEncoding());
			}
		}
	}

	// Execution definition (7)

	// If implied constraint required, we introduce variable x
	// if(pargs->getBoolOption(IMPLIED1) || pargs->getBoolOption(IMPLIED2) || pargs->getBoolOption(IMPLIED3)){
	for (int i = 1; i <= N; i++)
	{
		for (int t = ins->ES(i); t < ins->LC(i, ub); t++)
		{
			boolvar x = f->newBoolVar("x", i, t);
			boolvar s_i_t = t <= ins->LS(i, ub) ? f->bvar("s", i, t) : f->trueVar();
			f->addClause(!x | s_i_t);
			if (t - ins->getDuration(i) >= ins->ES(i))
			{
				boolvar s_i_t1 = f->bvar("s", i, t - ins->getDuration(i));
				f->addClause(!x | !s_i_t1);
				f->addClause(x | !s_i_t | s_i_t1);
			}
			else
			{
				f->addClause(x | !s_i_t);
			}
		}
	}

	// Resource capacity per time (17)

	// Each resource performs at most one skill at a time
	for (int k = 0; k < R; k++)
	{
		for (int t = 0; t <= ub; t++)
		{
			vector<literal> v;
			for (int i = 1; i <= N; i++)
			{
				if (ins->ES(i) <= t && t < ins->LC(i, ub) && ins->usefulToActivity(k, i))
				{
					boolvar x = f->newBoolVar("act_res_time", i, k, t);
					v.push_back(x);

					// Chanelling
					f->addClause(!x | f->bvar("x", i, t));
					f->addClause(!x | f->bvar("act_res", i, k));
					f->addClause(x | !f->bvar("act_res", i, k) | !f->bvar("x", i, t));
				}
			}
			f->addAMO(v, sargs->getAMOEncoding());
		}
	}

	// Implied 1: the number of activities requiring skill 'l' at a particular time
	//	is not greater than the number of resources that master 'l'
	if (pargs->getBoolOption(IMPLIED1))
	{
		for (int l = 0; l < L; l++)
		{
			for (int t = 0; t < ub; t++)
			{
				vector<vector<literal>> X;
				vector<vector<int>> Q;

				vector<int> vtasks;
				vector<set<int>> groups;

				for (int i = 1; i <= N; i++)
				{
					int ESi = ins->ES(i);
					int LCi = ins->LC(i, ub);
					if (t >= ESi && t < LCi && ins->demandsSkill(i, l))
						vtasks.push_back(i);
				}

				if (!vtasks.empty())
					ins->computeMinPathCover(vtasks, groups);

				for (const set<int> &group : groups)
				{
					vector<literal> vars_part;
					vector<int> coefs_part;

					for (int i : group)
					{
						vars_part.push_back(f->bvar("x", i, t));
						coefs_part.push_back(ins->getDemand(i, l));
					}

					if (!coefs_part.empty())
					{
						X.push_back(vars_part);
						Q.push_back(coefs_part);
					}
				}

				if (!X.empty())
				{
					f->addAMOPB(Q, X, ins->getNResourcesMastering(l), sargs->getAMOPBEncoding());
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

			vector<vector<literal>> X;
			vector<vector<int>> Q;

			vector<int> vtasks;
			vector<set<int>> groups;

			for (int i = 1; i <= N; i++)
			{
				int ESi = ins->ES(i);
				int LCi = ins->LC(i, ub);
				if (t >= ESi && t < LCi)
					vtasks.push_back(i);
			}

			if (!vtasks.empty())
				ins->computeMinPathCover(vtasks, groups);

			for (const set<int> &group : groups)
			{
				vector<literal> vars_part;
				vector<int> coefs_part;

				for (int i : group)
				{
					vars_part.push_back(f->bvar("x", i, t));
					coefs_part.push_back(ins->getTotalDemand(i));
				}

				if (!coefs_part.empty())
				{
					X.push_back(vars_part);
					Q.push_back(coefs_part);
				}
			}

			if (!X.empty())
			{
				util::sortCoefsDecreasing(Q, X);
				f->addAMOPB(Q, X, R, sargs->getAMOPBEncoding());
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
		int nCombinations = 1 << ins->getNSkills(); // (2^N_skills)
		// vector<bool> dominated(nCombinations,false);

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
			vector<int> vtasks;
			vector<set<int>> groups;

			vector<bool> required(ins->getNSkills(), false);

			for (int i = 1; i <= N; i++)
			{
				int ESi = ins->ES(i);
				int LCi = ins->LC(i, ub);
				if (t >= ESi && t < LCi)
				{
					vtasks.push_back(i);
					for (int l = 0; l < ins->getNSkills(); l++)
						if (ins->demandsSkill(i, l))
							required[l] = true;
				}
			}

			if (!vtasks.empty())
				ins->computeMinPathCover(vtasks, groups);

			// At each time instant, only consider the subsets that contain skills required
			// by some of the activities that can be running at this particular time

			// For each non-dominated combination of skills
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
					vector<vector<literal>> X;
					vector<vector<int>> Q;
					for (const set<int> &group : groups)
					{
						vector<literal> vars_part;
						vector<int> coefs_part;

						for (int i : group)
						{
							int demand = 0;
							for (int l = 0; l < ins->getNSkills(); l++)
								if (util::nthBit(combination, l))
									demand += ins->getDemand(i, l);

							if (demand != 0)
							{
								vars_part.push_back(f->bvar("x", i, t));
								coefs_part.push_back(demand);
							}
						}

						if (!coefs_part.empty())
						{
							X.push_back(vars_part);
							Q.push_back(coefs_part);
						}
					}

					if (!X.empty())
					{
						nImplied++;
						util::sortCoefsDecreasing(Q, X);
						f->addAMOPB(Q, X, capacity[combination], sargs->getAMOPBEncoding());
					}
				}
			}
		}
		// cout << "c nImpliedAsserted is " << nImplied << "/" << (ub * (1<<ins->getNSkills())) << endl;
	}

	return f;
}

void MSPSPTimeMaxSATEncoding::setModel(const EncodedFormula &ef, int lb, int ub, const vector<bool> &bmodel, const vector<int> &imodel)
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

bool MSPSPTimeMaxSATEncoding::narrowBounds(const EncodedFormula &ef, int lastLB, int lastUB, int lb, int ub)
{
	int N = ins->getNActivities();
	if (ub <= lastUB)
	{
		ef.f->addClause(ef.f->ivar("S", N + 1) <= ub);
		ef.f->addClause(ef.f->ivar("S", N + 1) >= lb);
		return true;
	}
	else
		return false;
}

void MSPSPTimeMaxSATEncoding::assumeBounds(const EncodedFormula &ef, int lb, int ub, vector<literal> &assumptions)
{
	int N = ins->getNActivities();
	assumptions.push_back(ef.f->ivar("S", N + 1) <= ub);
	assumptions.push_back(ef.f->ivar("S", N + 1) >= lb);
}

MSPSPModel MSPSPTimeMaxSATEncoding::makeModel(const EncodedFormula &ef, const istream &bmodel) const
{
	vector<bool> model;
	readModel(const_cast<istream &>(bmodel), ' ', model);
	return MSPSPEncoding::_makeModel(ef, model);
}

// returns true if found (and consumes the char), false otherwise
bool MSPSPTimeMaxSATEncoding::findChar(std::istream &model_str, char c) const
{
	char aux;
	while (model_str.get(aux))
	{ // stop if read fails (EOF or error)
		if (aux == c)
			return true;
	}
	return false;
}

void MSPSPTimeMaxSATEncoding::readModel(istream &model_str, char c, vector<bool> &model) const
{
	findChar(model_str, c); // Consume until marker

	if (!model_str || model_str.eof())
	{
		cerr << "Error: no model found in the input stream." << endl;
		exit(BADFILE_ERROR);
	}

	char ch;
	while (model_str.get(ch))
	{
		if (ch == '0')
			model.push_back(false);
		else if (ch == '1')
			model.push_back(true);
		else if (isspace(ch))
			continue; // ignore spaces
		else
			break; // stop on anything not 0/1
	}
}