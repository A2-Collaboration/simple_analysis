#include "SessionManager.h"
#include <fstream>

void SessionManager::Save(const std::string& file, int i, double x1, double x2)
{
    std::ofstream f(file);
    f << i << " " << x1 << " " << x2;
}

bool SessionManager::Load(const std::string& file, int& i, double& x1, double& x2)
{
    std::ifstream f(file);
    if(!f) return false;
    f >> i >> x1 >> x2;
    return true;
}
