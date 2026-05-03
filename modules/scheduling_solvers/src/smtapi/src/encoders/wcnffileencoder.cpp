#include "wcnffileencoder.h"
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

WCNFFileEncoder::WCNFFileEncoder(Encoding * enc, const std::string & solver) : FileEncoder(enc){
	if(solver=="yices")
		this->solver="yices-sat";
	else if(solver=="glucose")
		this->solver="glucose_release";
	else if(solver=="openwbo")
		this->solver="open-wbo_release";
	else{
		std::cerr << "Unsupported solver " << solver << std::endl;
		exit(BADARGUMENTS_ERROR);
	}
}

WCNFFileEncoder::~WCNFFileEncoder(){

}

std::string WCNFFileEncoder::getCall() const{
	if(produceModels()){
		if(solver == "glucose_release")
			return solver + " -model " + getTMPFileName() + " | grep -E '(^s )|(^v )'";
		else if(solver == "yices-sat")
			return solver + " -m " + getTMPFileName() + " | tail -n 2";
	}
	else if(solver == "glucose_release") //Apanyu momentani
		return solver + " " + getTMPFileName() + " | grep -E '(^s )|(^c CPU time)' | cut -d ':' -f 2 | sed -e 's/s//g'";
	else return solver + " " + getTMPFileName() + " | grep -E '(^s )|(^v )'";
}



void WCNFFileEncoder::createFile(std::ostream & os, SMTFormula * f) const{

	if(f->getType() != SATFORMULA && f->getType() != MAXSATFORMULA)
	{
		std::cerr << "Expected SAT or MaxSAT formula" << std::endl;
		exit(BADCODIFICATION_ERROR);
	}

	if(f->hasSoftClausesWithVars())
	{
		std::cerr << "Error: standard MaxSAT does not accept soft clauses with associated variables" << std::endl;
		exit(BADCODIFICATION_ERROR);
	}

	for (int i = 0; i < 3; i++)
	{
		if (i == 0)
			// Required to perform memory allocation
			os << "c " << "#variables= " << f->getNBoolVars()
			   << " " << "#clauses= " << f->getNClauses() << std::endl;
		else
			os << "c " << std::endl;
	}

	for (int i = 0; i < 3; i++)
	{
		if (i == 0)
			// Required to perform memory allocation
			os << "c " <<  "#obj_size= " << f->getNSoftClauses() << std::endl;
		else
			os << std::endl;
	}

	for (const clause &c : f->getClauses())
	{
		os << "h ";
		for (const literal &l : c.v)
		{
			if (l.arith)
			{
				std::cerr << "Error: attempted to add arithmetic literal to SAT encoding" << std::endl;
				exit(BADCODIFICATION_ERROR);
			}

			if (l.v.id <= 0 || l.v.id > f->getNBoolVars())
			{
				std::cerr << "Error: asserted undefined Boolean variable" << std::endl;
				exit(UNDEFINEDVARIABLE_ERROR);
			}

			os << (l.sign ? l.v.id : -l.v.id) << " ";
		}
		os << "0" << std::endl;
	}

	for(int i = 0; i < f->getNSoftClauses(); i++)
	{
		const clause & c = f->getSoftClauses()[i];
		os << f->getWeights()[i] << " ";
		for(const literal & l : c.v){
			if(l.arith){
				std::cerr << "Error: attempted to add arithmetic literal to SAT encoding"<< std::endl;
				exit(BADCODIFICATION_ERROR);
			}

			if(l.v.id <= 0 || l.v.id>f->getNBoolVars()){
				std::cerr << "Error: asserted undefined Boolean variable"<< std::endl;
				exit(UNDEFINEDVARIABLE_ERROR);
			}

			os << (l.sign ? l.v.id : -l.v.id) << " ";
		}
		os << "0" << std::endl;
	}

}


void WCNFFileEncoder::createMappingFile(std::ostream &os, SMTFormula *f) const {
	const std::vector<std::string> &boolVarNames = f->getBoolVarNames();
	for (int i = 1; i < boolVarNames.size(); i++)
	{
		std::string varName = boolVarNames[i];
		os << "c " << i << " " << varName << std::endl;
	}
}
