#pragma once

#include <string>
#include <tuple>

namespace telebit::internal {

// Applies IME escape rules (literal passthrough with collapse). Returns true if triggered.
bool applyEscapeRules(const std::string& word, const std::string& lower, std::string& outRaw);

// Normalizes English-like triple-vowel runs without turning them into Vietnamese shapes.
// Returns true if any change was made.
bool normalizeTripleVowels(std::string& s);

// Splits a Telex body into onset + raw rime (lowercase).
void splitOnsetRime(const std::string& body, std::string& onset, std::string& rimeRaw);

// Extracts the tone key (if any) and returns (body_without_tone_keys, tone, strip=false).
std::tuple<std::string, int, bool> extractTone(const std::string& wordLower);

// Returns a canonical raw-rime Telex spelling that matches the rime table when possible.
std::string canonicalizeRimeByTable(const std::string& rimeRaw);

// Converts a raw rime Telex spelling into an internal shaped form (placeholders A/B/E/O/Q/U/D/W).
std::string applyShapesRime(std::string rimeRaw);

}  // namespace telebit::internal

