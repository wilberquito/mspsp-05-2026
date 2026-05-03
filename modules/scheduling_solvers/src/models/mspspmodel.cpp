#include "mspspmodel.h"


MSPSPModel::MSPSPModel() { }

MSPSPModel::MSPSPModel(const std::vector<int> & starts, const std::vector<std::vector<std::pair<int, int>>> & assignment) {
    this->starts = starts;
    this->assignment = assignment;
}

const std::vector<int> & MSPSPModel::getStarts() const {
    return starts;
}

const std::vector<std::vector<std::pair<int, int>>> & MSPSPModel::getAssignment() const {
    return assignment;
}

