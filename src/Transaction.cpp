#include "Transaction.h"
#include <sstream>

std::string Transaction::toString() const {
    std::ostringstream oss;
    oss << "Type: " << type << ", Amount: " << amount << ", Details: " << details << ", Resulting Balance: " << resultingBalance;
    return oss.str();
}
