#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>

#include "Bank.h"

// Reads and writes the save file. The only module that touches the disk.
//
// Format, one record per line, every field escaped:
//   ACC,<number>,<name>,<saltHex>,<hashHex>,<balancePence>,<failedAttempts>,<lockedAtEpoch>
//   TXN,<type>,<amountPence>,<details>,<resultingBalancePence>
//
// TXN records attach to the most recent ACC. The record prefix means the
// parser knows what it is reading rather than inferring it from whether a
// conversion threw.
namespace storage {

struct SaveOutcome {
    bool ok{false};
    std::string error;
};

struct LoadOutcome {
    bool ok{false};
    std::string error;
    int line{0};
};

SaveOutcome save(const Bank& bank, const std::string& path);

// A missing file is not an error: it means a first run. Anything malformed
// is, and the bank is left untouched rather than partially populated.
LoadOutcome load(Bank& bank, const std::string& path);

// Escapes backslash first, then comma, so the transformation is reversible.
std::string escapeField(const std::string& field);
std::string unescapeField(const std::string& field);
std::vector<std::string> splitEscaped(const std::string& line);

}  // namespace storage

#endif
