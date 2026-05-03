#ifndef MSPSPMODEL_DEF
#define MSPSPMODEL_DEF

#include <vector>
#include <iostream>

class MSPSPModel {

   private:
      std::vector<int> starts;
      std::vector<std::vector<std::pair<int, int>>> assignment;


    public:
        MSPSPModel();

        MSPSPModel(const std::vector<int> & starts, const std::vector<std::vector<std::pair<int, int>>> & assignment);

        const std::vector<int> & getStarts() const;

        const std::vector<std::vector<std::pair<int, int>>> & getAssignment() const;
};

#endif
