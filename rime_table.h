#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vnkey::internal {

// Internal vowel placeholders used by the Telex engine.
// A=ă, B=â, E=ê, O=ô, Q=ơ, U=ư.
inline constexpr std::string_view kInternalVowels = "aeiouyABEOQU";

// Returns the ordered list of valid Vietnamese onsets (longest-first).
const std::vector<std::string>& getOnsets();

// Returns the main-vowel index table for each rime (internal form).
// Key is the shaped rime string starting from the first vowel, e.g. "UQi", "yEt".
const std::unordered_map<std::string, int>& getRimeMainVowelTable();

}  // namespace vnkey::internal

