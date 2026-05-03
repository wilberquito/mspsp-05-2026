#ifndef WCNFFILEENCODERPRE21_DEFINITION
#define WCNFFILEENCODERPRE21_DEFINITION

#include "fileencoder.h"
#include "smtformula.h"
#include <iostream>

using namespace smtapi;



/*
 * This class codifies a MaxSAT formula into the MaxSAT evaluation standard format (previous to MSE 21)
 * If the formula contains theory literals, a panic exit will
 * occur with error code BADCODIFICATION_ERROR. If it cannot be
 * written into the specified destination file, the application,
 * a panic exit will occur with error code BADFILE_ERROR.
 */
class WCNFFileEncoderPre21 : public FileEncoder {
private:
	
	std::string solver;
	
	std::string getCall() const;
	
public:	
  
	//Constructor
	WCNFFileEncoderPre21(Encoding * enc, const std::string &solver); 
	
	//Destructor
	~WCNFFileEncoderPre21();

	virtual void createFile(std::ostream & os, SMTFormula * f) const;
	
};

#endif

