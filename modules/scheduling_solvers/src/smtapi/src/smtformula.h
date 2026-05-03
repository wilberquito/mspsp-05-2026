#ifndef SMTFORMULA_DEFINITION
#define SMTFORMULA_DEFINITION

#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <sstream>
#include <stdlib.h>
#include "mdd.h"
#include "smtapi.h"
#include "mddbuilder.h"
#include "errors.h"

namespace smtapi
{

	/*
	 * This class is meant to manage the creation and assertion of
	 * a CNF SMT formula.
	 * It only handles the theory of LIA.
	 * Variables are identified by a name and up to 3 subindices.
	 * It also provides unnamed variables, which will usually be auxilliary variables
	 * which are going to be used only once.
	 * It is a pure virtual class which delegates to child classes
	 * the assertion of formulas, either to solver APIs or to standard files.
	 * It implements SAT encodings of many common constraints:
	 * 	- at-most-one
	 * 	- at-least-one
	 * 	- exactly-one
	 * 	- cardinality constraint
	 * 	- pseudo-Boolean constraint
	 * 	- at-most-one pseudo-Boolean constraint
	 */

	enum FORMULA_TYPE
	{
		SATFORMULA,
		MAXSATFORMULA,
		OPBFORMULA,
		PBFORMULA,
		SMTFORMULA,
		OMTMINFORMULA,
		OMTMAXFORMULA
	};

	enum AMOEncoding
	{
		AMO_QUAD,
		AMO_LOG,
		AMO_LADDER,
		AMO_HEULE,
		AMO_COMMANDER, // Draft, not tested
	};

	enum CardinalityEncoding
	{
		CARD_SORTER,
		CARD_TOTALIZER,
		CARD_SC
	};

	enum PBEncoding
	{
		PB_LIA,
		PB_BDD,
		PB_BDDIO,
		PB_SWC,
		PB_GT,
		PB_RGT,
		PB_RGTnoR,
		PB_RGTnoPre,
		PB_MTO,
		PB_GPW,
		PB_LPW,
		PB_GBM,
		PB_LBM
	};

	enum AMOPBEncoding
	{
		AMOPB_LIA,
		AMOPB_BDD,
		AMOPB_BDDIO,
		AMOPB_SWC,
		AMOPB_GT,
		AMOPB_RGT,
		AMOPB_RGTnoR,
		AMOPB_RGTnoPre,
		AMOPB_MTO,
		AMOPB_GPW,
		AMOPB_LPW,
		AMOPB_GBM,
		AMOPB_LBM,
		AMOPB_AMOMDD,
		AMOPB_AMOMDDIO,
		AMOPB_IMPCHAIN,
		AMOPB_AMOBDD,
		AMOPB_GSWC,
		AMOPB_SORTER,
		AMOPB_GGT,
		AMOPB_RGGT,
		AMOPB_RGGTnoR,	 // Do not reduce
		AMOPB_RGGTnoPre, // Do not pre-process equal coefficients in AMO
		AMOPB_GMTO,
		AMOPB_GMTO2,
		AMOPB_GGPW,
		AMOPB_GLPW,
		AMOPB_GGBM,
		AMOPB_GLBM
	};

	extern std::map<AMOPBEncoding, PBEncoding> amopb_pb_rel;

	struct RGGTNode
	{
		RGGTNode *left;
		RGGTNode *right;
		RGGTNode *parent;

		std::vector<int> values;
		std::vector<std::pair<int, int>> intervals;
		std::vector<literal> literals;

		RGGTNode();
		RGGTNode(RGGTNode *left, RGGTNode *right);

		~RGGTNode();

		int fuoGetDepth();

		void makeIntervals(int K, bool reduce);
	};

	struct PBconstraint
	{
		std::vector<int> q;
		std::vector<literal> x;
		int k;
		bool isEQ;
		PBconstraint(const std::vector<int> &q, const std::vector<literal> &x, int k, bool isEQ) : q(q), x(x), k(k), isEQ(isEQ) {}
	};

	struct PBgoal
	{
		std::vector<int> q;
		std::vector<literal> x;
		PBgoal(const std::vector<int> &q, const std::vector<literal> &x) : q(q), x(x) {}
	};

	class SMTFormula
	{

	private:
		/*
		 * Boolean/Int variables are identified in the ranges [1,nBoolVars] and
		 * [1,nIntVars] respectively. Any variable with an identifier out of this
		 * range will cause a panic exit in time of solver API/file formula assertion,
		 * with error code UNDEFINEDVARIABLE_ERROR.
		 */
		int nBoolVars; // Number of Boolean variables
		int nIntVars;  // Number of Int variables

		int nClauses;						// Number of clauses
		std::vector<clause> clauses;		// Vector of clauses
		std::vector<clause> softclauses;	// Vector of soft clauses. If non-empty, is a partial MaxSat problem
		std::vector<int> weights;			// Vector of weights of the soft clauses.
		std::vector<intvar> softclausevars; // Vector of soft clauses.

		std::vector<PBconstraint> pbconstraints; // Vector of pseudo-Boolean constraints
		PBgoal *pbGoal;							 // Pseudo-Boolean optimization objective function

		std::map<std::string, boolvar> mapBoolVars; // Map of Boolean variables identified by name
		std::map<std::string, intvar> mapIntVars;	// Map of Int variables identified by name

		std::vector<std::string> boolVarNames; // Name of Boolvars indexed by id. Position 0 is "".
		std::vector<std::string> intVarNames;  // Name of Intvars indexed by id. Position 0 is "".
		std::vector<bool> declareVar;		   // True iff if the i-th int var is a variable has to be declared (i.e. is not a soft clause var)

		static std::string defaultauxboolvarpref; // Default prefix for auxilliary bool variables names
		static std::string defaultauxintvarpref;  // Default prefix for auxilliary int variables names
		std::string auxboolvarpref;				  // Prefix for auxilliary bool variables names
		std::string auxintvarpref;				  // Prefix for auxilliary int variables names

		std::map<int, intvar> mapLiteralToInt; // Mapping from boolvarId to equivalent 0/1 integer variables

		bool use_predef_lits;
		bool use_predef_order;
		std::vector<literal> predef_lits;

		boolvar falsevar; // Singleton trivially false variable
		boolvar truevar;  // Single trivially true variable

		intsum objFunc;				 // Objective function in case it is OMT
		bool hasObjFunc;			 // True iff an objective function has been defined
		bool hassoftclauseswithvars; // True if some soft clause has an associated variable
		bool isMinimization;		 // True if is an OMT minimization problem
		int LB;						 // Lower bound for the objective function
		int UB;						 // Upper bound for the objective function

		// Checks if a boolean variable with given name exists
		bool _bExists(const std::string &name) const;

		boolvar _bvar(const std::string &s) const;

		void addOrderEncoding(int x, std::vector<literal> &lits);

		// Adds the codification of Sorter [x1,x2] -> [y1,y2]
		void addTwoComparator(const literal &x1, const literal &x2, literal &y1, literal &y2, bool leqclauses, bool geqclauses);

		// Adds the codification of "y is the result of merging x1,x2". Used in cardinality constraint
		void addSimplifiedMerge(const std::vector<literal> &x1, const std::vector<literal> &x2, std::vector<literal> &y, int c, bool leqclauses, bool geqclauses);

		void addQuadraticMerge(const std::vector<literal> &x1, const std::vector<literal> &x2, std::vector<literal> &y);

		void addTotalizer(const std::vector<literal> &x, std::vector<literal> &y);

		void addTotalizer(const std::vector<literal> &x, const std::vector<std::pair<int, std::set<int>>> &inputBits, std::vector<literal> &y, int lIndex,
						  int partSize, int ommittedLeaf,
						  std::map<std::set<std::pair<int, int>>, std::vector<literal>> &constructed);

		void addTotalizer(const std::vector<literal> &x, std::vector<literal> &y, int k);

		literal assertMDDLEQAbio(MDD *mdd);

		literal assertMDDLEQAbio(MDD *mdd, std::vector<literal> &asserted);

		literal assertMDDGTAbio(MDD *mdd);

		literal assertMDDGTAbio(MDD *mdd, std::vector<literal> &asserted, std::vector<literal> &elses);

		void addPBLIA(const std::vector<int> &Q, const std::vector<literal> &X, int K);

		void addAMKSequentialCounter(const std::vector<literal> &x, int K);

		void addAMOPBSWC(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K);

		void addAMOPBSorter(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K);

		void addAMOPBGeneralizedTotalizer(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K);

		void addAMOPBReducedGeneralizedGeneralizedTotalizer(std::vector<std::vector<int>> Q, std::vector<std::vector<literal>> X, int K, bool reduce);

		void mergeValues(const std::vector<int> &n1, const std::vector<int> &n2, std::vector<int> &n3, int K);

		literal MTOVar(std::map<int, literal> &m, int w);

		void baseSelectionMTO(const std::vector<std::vector<int>> &Q, int K, std::vector<int> &moduli);

		void baseSelectionMTO2(const std::vector<std::vector<int>> &Q, int K, std::vector<int> &moduli);

		void addAMOPBModuloTotalizer(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K);

		literal addPolynomialWatchdog(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K, bool useSorter);

		void addAMOPBLocalPolynomialWatchdog(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K);

		void addAMOPBNaiveLocalPolynomialWatchdog(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K, bool useSorter);

		void addAMOPBGlobalPolynomialWatchdog(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K, bool useSorter);

		// Add a PB constraint to the formula without encoding it to CNF
		// op is 0 for >=, op is 1 for <=, op is 2 for =
		void addPBconstraint(const std::vector<int> &Q, const std::vector<literal> &X, int K, int op = 0);

		std::string ssubs(const std::string &var, int i1) const;
		std::string ssubs(const std::string &var, int i1, int i2) const;
		std::string ssubs(const std::string &var, int i1, int i2, int i3) const;

	public:
		void comtMTO(int K, const std::vector<int> &moduli, const std::vector<std::map<int, literal>> &D, literal *localLit);

		void nLevelsMTO(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int lIndex, int partSize,
						const std::vector<int> &moduli, std::vector<std::map<int, literal>> &D);

		// Default constructor
		SMTFormula();

		// Destructor
		~SMTFormula();

		// Return the type of the formula based on its content
		FORMULA_TYPE getType() const;

		bool hasSoftClausesWithVars() const;

		int getNBoolVars() const;

		int getNIntVars() const;

		int getNClauses() const;

		int getNSoftClauses() const;

		const std::vector<clause> &getClauses() const;

		const std::vector<PBconstraint> &getPBconstraints() const;

		const PBgoal *getPBgoal() const;

		const int getNPBconstraints() const;

		const std::vector<clause> &getSoftClauses() const;

		const std::vector<int> &getWeights() const;

		const std::vector<intvar> &getSoftClauseVars() const;

		int getHardWeight() const;

		const std::vector<std::string> &getBoolVarNames() const;

		const std::vector<std::string> &getIntVarNames() const;

		void setAuxBoolvarPref(const std::string &s);

		void setAuxIntvarPref(const std::string &s);

		void setDefaultAuxBoolvarPref();

		void setDefaultIntvarPref();

		bool usePredefDecs() const;

		bool usePredefOrder() const;

		void setUsePredefDecs(const std::vector<literal> &lits, bool order);

		void getPredefDecs(std::vector<literal> &lits) const;

		bool isDeclareVar(int id) const;

		const intsum &getObjFunc() const;

		// Get the trivially false variable
		boolvar falseVar();

		// Get the trivially true variable
		boolvar trueVar();

		// The name of a variable cannot contain the character "_"

		// Get a new unnamed Boolean variable
		boolvar newBoolVar();
		// Get a new named Boolean variable, with up to 3 subindices in the name
		boolvar newBoolVar(const std::string &var);
		boolvar newBoolVar(const std::string &var, int i1);
		boolvar newBoolVar(const std::string &var, int i1, int i2);
		boolvar newBoolVar(const std::string &var, int i1, int i2, int i3);

		// Set an alias for a boolvar
		void aliasBoolVar(const boolvar &x, const std::string &var);
		void aliasBoolVar(const boolvar &x, const std::string &var, int i1);
		void aliasBoolVar(const boolvar &x, const std::string &var, int i1, int i2);
		void aliasBoolVar(const boolvar &x, const std::string &var, int i1, int i2, int i3);

		// Get a new unnamed Int variable
		intvar newIntVar(bool declare = true);
		// Get a new named Int variable, with up to 3 subindices in the name
		intvar newIntVar(const std::string &var, bool declare = true);
		intvar newIntVar(const std::string &var, int i1, bool declare = true);
		intvar newIntVar(const std::string &var, int i1, int i2, bool declare = true);
		intvar newIntVar(const std::string &var, int i1, int i2, int i3, bool declare = true);

		// Set an alias for an intvar
		void aliasIntVar(const intvar &x, const std::string &var);
		void aliasIntVar(const intvar &x, const std::string &var, int i1);
		void aliasIntVar(const intvar &x, const std::string &var, int i1, int i2);
		void aliasIntVar(const intvar &x, const std::string &var, int i1, int i2, int i3);

		// Get named Boolean variable by name and subindices
		boolvar bvar(const std::string &var) const;
		boolvar bvar(const std::string &var, int i1) const;
		boolvar bvar(const std::string &var, int i1, int i2) const;
		boolvar bvar(const std::string &var, int i1, int i2, int i3) const;

		// Check if a boolean variable with given name exists
		bool bvarExists(const std::string &var) const;
		bool bvarExists(const std::string &var, int i1) const;
		bool bvarExists(const std::string &var, int i1, int i2) const;
		bool bvarExists(const std::string &var, int i1, int i2, int i3) const;

		// Get named Int variable by name and subindices
		intvar ivar(const std::string &var) const;
		intvar ivar(const std::string &var, int i1) const;
		intvar ivar(const std::string &var, int i1, int i2) const;
		intvar ivar(const std::string &var, int i1, int i2, int i3) const;

		void minimize(const intsum &sum);
		void maximize(const intsum &sum);
		void setLowerBound(int LB);
		void setUpperBound(int UB);
		int getLowerBound();
		int getUpperBound();

		static int getIValue(const intvar &var, const std::vector<int> &vals);
		static bool getBValue(const boolvar &var, const std::vector<bool> &vals);

		// Add the empty clause to the formula
		void addEmptyClause();

		// Add clause 'c' to the formula
		void addClause(const clause &c);

		void addPBconstraint(const std::vector<int> &Q, const std::vector<literal> &X, std::string op, int K);

		void addPBconstraint(const clause &c);

		// Add a PB objective to the formula. To be used in OPB solver in as
		// function to minimize such as "minimize(1*x1 + 1*x2 + 1*x3 ... + 1*xN)".
		void addPBgoal(const std::vector<literal> &X);

		// Add a PB objective to the formula. To be used in OPB solver in as
		// function to minimize such as "minimize(q1*x1 + q2*x2 + q3*x3 ... + qN*xN)".
		void addPBgoal(const std::vector<int> &Q, const std::vector<literal> &X);

		// Add implied pseudo-Boolean: y -> \sum reified >= K
		void addImpliedPBCardinality(literal y, std::vector<literal> reified, int K);

		// Adds reified pseudo-Boolean: y <-> \sum reified >= K
		void addPBCardinalityReification(literal y, std::vector<literal> reified, int K);

		// Adds reified pseudo-Boolean AND: y <-> \sum reified >= |reified|
		void addPBAndReification(const literal &y, const std::vector<literal> &reified);

		// Adds reified pseudo-Boolean OR: y <-> \sum reified >= 1
		void addPBOrReification(const literal &y, const std::vector<literal> &reified);

		// Add soft clause 'c' to the formula
		void addSoftClause(const clause &c, int weight = 1);

		// Add soft clause 'c' to the formula
		void addSoftClauseWithVar(const clause &c, int weight, const intvar &var);

		// All all the clauses in 'v' to the formula
		void addClauses(const std::vector<clause> &c);

		// Adds at-least-one constraint on the literals in 'v'
		void addALO(const std::vector<literal> &v);

		// Adds at-most-one constraint on the literals in 'v'
		void addAMO(const std::vector<literal> &v, AMOEncoding enc = AMO_QUAD);

		// Adds exactly-one constraint on the literals in 'v'
		void addEO(const std::vector<literal> &v, AMOEncoding enc = AMO_QUAD);

		// Adds at-least-K constraint on the literals in 'v'
		void addALK(const std::vector<literal> &v, int K, CardinalityEncoding enc = CARD_SORTER);

		// Adds at-most-K constraint on the literals in 'v'
		void addAMK(const std::vector<literal> &v, int K, CardinalityEncoding enc = CARD_SORTER);

		// Adds exactly-K constraint on the literals in 'v'
		void addEK(const std::vector<literal> &v, int K);

		// Adds PB constraint Q*X <= K
		void addPB(const std::vector<int> &Q, const std::vector<literal> &X, int K, PBEncoding = PB_BDD);

		// Adds PB constraint Q*X >= K
		void addPBGEQ(const std::vector<int> &Q, const std::vector<literal> &X, int K, PBEncoding = PB_BDD);

		// Adds AMO-PB constraint Q*X <= K
		void addAMOPB(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K, AMOPBEncoding encoding = AMOPB_AMOMDD);

		// Adds AMO-PB constraint Q*X >= K
		void addAMOPBGEQ(const std::vector<std::vector<int>> &Q, const std::vector<std::vector<literal>> &X, int K, AMOPBEncoding encoding = AMOPB_AMOMDD);

		// Adds the codification of "y is the x list sorted  decreasingly". Used in cardinality constraint
		void addSorting(const std::vector<literal> &x, std::vector<literal> &y, bool leqclauses, bool geqclauses);

		// Adds the codification of "y contains the first m bits of the x list sorted decreasingly". Used in cardinality constraint
		// If the length of x is smaller than m, the length of y is the same as the length of x
		void addMCardinality(const std::vector<literal> &x, std::vector<literal> &y, int m, bool leqclauses, bool geqclauses);

		// Adds the codification of "y is the result of merging x1,x2". Used in cardinality constraint
		void addMerge(const std::vector<literal> &x1, const std::vector<literal> &x2, std::vector<literal> &y, bool leqclauses, bool geqclauses);

		void addGerarquicAMOAMK(const std::vector<std::vector<std::vector<int>>> &Q,
								const std::vector<std::vector<std::vector<literal>>> &X,
								const std::vector<int> &capacities);

		void addAMOPBCardialitySWC(const std::vector<std::vector<int>> &Q,
								   const std::vector<std::vector<literal>> &X, std::vector<literal> &output, int K);

		void addQuadraticMergeCardinality(const std::vector<literal> &x1, const std::vector<literal> &x2, std::vector<literal> &y, int K);

		// Add implied at-most-K constraint: y -> ALK(v, K)
		void addImpliedALK(const literal &y, const std::vector<literal> &v, int K, CardinalityEncoding enc = CARD_SORTER);
	};

}

#endif
