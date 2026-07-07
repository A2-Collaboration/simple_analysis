#pragma once
#include <string>

class SessionManager
{
public:
    void Save(const std::string& file, int idx, double xmin, double xmax);
    bool Load(const std::string& file, int& idx, double& xmin, double& xmax);
};
