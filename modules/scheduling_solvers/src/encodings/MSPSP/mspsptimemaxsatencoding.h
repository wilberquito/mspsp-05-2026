#ifndef MSPSPTIMEMAXSATENCODING_DEFINITION
#define MSPSPTIMEMAXSATENCODING_DEFINITION


#include <string>
#include <vector>
#include "mspsp.h"
#include "smtformula.h"
#include "mspspencoding.h"
#include "solvingarguments.h"
#include "arguments.h"

using namespace std;
using namespace smtapi;

class MSPSPTimeMaxSATEncoding : public MSPSPEncoding {
private:

	void readModel(istream & model_str, char sep, vector<bool> & model) const;
	bool findChar(istream & model_str, char c) const;

protected:

  Arguments<ProgramArg> * pargs;

  SolvingArguments * sargs;


public:

MSPSPTimeMaxSATEncoding(MSPSP * instance, SolvingArguments * sargs, Arguments<ProgramArg> * pargs);
	~MSPSPTimeMaxSATEncoding();


	SMTFormula * encode(int lb = INT_MIN, int ub = INT_MAX);
	void setModel(const EncodedFormula & ef, int lb, int ub, const vector<bool> & bmodel, const vector<int> & imodel);
	bool narrowBounds(const EncodedFormula & ef, int lastLB, int lastUB, int lb, int ub);
	void assumeBounds(const EncodedFormula & ef, int LB, int ub, vector<literal> & assumptions);

	MSPSPModel makeModel(const EncodedFormula &ef, const istream &bmodel) const override;
};

#endif

