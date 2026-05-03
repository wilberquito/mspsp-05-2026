#include "opbfileencoder.h"
#include <iostream>
#include <fstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include "errors.h"
#include <iterator>
#include <cassert>

using namespace smtapi;

OPBFileEncoder::OPBFileEncoder(Encoding *enc, const std::string &solver) : FileEncoder(enc)
{
	if (solver == "yices")
		this->solver = "yices-sat";
	else if (solver == "glucose")
		this->solver = "glucose_release";
	else if (solver == "openwbo")
		this->solver = "open-wbo_release";
	else
	{
		std::cerr << "Unsupported solver " << solver << std::endl;
		exit(BADARGUMENTS_ERROR);
	}
}

OPBFileEncoder::~OPBFileEncoder()
{
}

std::string OPBFileEncoder::getCall() const
{
	if (produceModels())
	{
		if (solver == "glucose_release")
			return solver + " -model " + getTMPFileName() + " | grep -E '(^s )|(^v )'";
		else if (solver == "yices-sat")
			return solver + " -m " + getTMPFileName() + " | tail -n 2";
	}
	else if (solver == "glucose_release") // Apanyu momentani
		return solver + " " + getTMPFileName() + " | grep -E '(^s )|(^c CPU time)' | cut -d ':' -f 2 | sed -e 's/s//g'";
	else
		return solver + " " + getTMPFileName() + " | grep -E '(^s )|(^v )'";
}

void OPBFileEncoder::createFile(std::ostream &os, SMTFormula *f) const
{
	if (f->getType() != FORMULA_TYPE::OPBFORMULA && f->getType() != FORMULA_TYPE::PBFORMULA) {
		std::cerr << "Expected OPB formula" << std::endl;
		exit(BADCODIFICATION_ERROR);
	}

	for (int i = 0; i < 3; i++)
	{
		if (i == 0)
			// Required to perform memory allocation
			os << "* " << "#variable= " << f->getNBoolVars()
			   << " " << "#constraint= " << f->getNPBconstraints() << std::endl;
		else
			os << "* " << std::endl;
	}

	// Maybe there is no PB optimization, but if there is, it must be the first thing in the file
	if (f->getType() == FORMULA_TYPE::OPBFORMULA){
		const PBgoal * ptr = f->getPBgoal();
		if (ptr == nullptr)
		{
			std::cerr << "Expected OPB formula with an objective function" << std::endl;
			exit(BADCODIFICATION_ERROR);
		}

		PBgoal goal = *ptr;

		std::vector<int> q = goal.q;
		std::vector<literal> x = goal.x;

		for (int i = 0; i < 3; i++)
		{
			if (i == 0)
				os << "* " << "#obj_size= " << x.size() << std::endl;
			else
				os << "* " << std::endl;
		}

		os << "min: ";
		for (int i = 0; i < q.size(); i++)
		{

	#ifndef NDEBUG
			assert(x[i].arith == false); // Literals are not arithmetic
			assert(x[i].sign == true);	 // Objective function cannot have negated literals
	#endif

			if (q[i] == 0)
				continue; // Skip zero coefficients
			if (q[i] > 0)
				os << '+';
			os << q[i] << " x" << x[i].v.id << " ";
		}
		os << "; " << std::endl;

	}


	for (const PBconstraint &c : f->getPBconstraints())
	{
		for (int i = 0; i < c.q.size(); i++)
		{
			assert(!c.x[i].arith);		 // PB constraints cannot have arithmetic literals
			assert(c.x[i].sign == true); // PB constraints cannot have negated literals
			if (c.q[i] >= 0)
				os << '+';
			os << c.q[i] << " x" << c.x[i].v.id << " ";
		}
		if (c.isEQ)
			os << "= ";
		else
			os << ">= ";
		os << c.k << " ;" << std::endl;
	}
}

void OPBFileEncoder::createMappingFile(std::ostream &os, SMTFormula *f) const
{
	const std::vector<std::string> &boolVarNames = f->getBoolVarNames();
	for (int i = 1; i < boolVarNames.size(); i++)
	{
		std::string varName = boolVarNames[i];
		os << "c " << f->bvar(varName).id << " " << varName << std::endl;
	}
}
