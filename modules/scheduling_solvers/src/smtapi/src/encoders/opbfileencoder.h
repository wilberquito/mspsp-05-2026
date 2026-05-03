#ifndef PBOFILEENCODER_DEFINITION
#define PBOFILEENCODER_DEFINITION

#include "fileencoder.h"
#include "smtformula.h"
#include <iostream>

using namespace smtapi;



/*
 * This class codifies a MaxSAT formula into the MaxSAT evaluation standard format (after MSE 21)
 * If the formula contains theory literals, a panic exit will
 * occur with error code BADCODIFICATION_ERROR. If it cannot be
 * written into the specified destination file, the application,
 * a panic exit will occur with error code BADFILE_ERROR.
 */
class OPBFileEncoder : public FileEncoder {
private:

	std::string solver;

	std::string getCall() const;

public:

	//Constructor
	OPBFileEncoder(Encoding * enc, const std::string &solver);

	//Destructor
	~OPBFileEncoder();
	virtual void createFile(std::ostream & os, SMTFormula * f) const;

	virtual void createMappingFile(std::ostream &os, SMTFormula *f) const override;

};

#endif

