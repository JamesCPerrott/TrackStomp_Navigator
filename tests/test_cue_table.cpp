#include "config.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#ifndef PRD_PATH
#error "PRD_PATH is not defined"
#endif

namespace {

struct ParsedCue {
    uint8_t note{};
    CueTrigger trigger{};
    uint8_t button_a{};
    uint8_t button_b{};
};

std::string trim(std::string text) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

bool parse_u8(const std::string& text, uint8_t& out) {
    if (text.empty()) {
        return false;
    }
    unsigned int value = 0;
    std::stringstream stream(text);
    stream >> value;
    if (stream.fail()) {
        return false;
    }
    char extra = 0;
    if (stream >> extra) {
        return false;
    }
    if (value > 255U) {
        return false;
    }
    out = static_cast<uint8_t>(value);
    return true;
}

bool ends_with(const std::string& text, const std::string& suffix) {
    if (text.size() < suffix.size()) {
        return false;
    }
    return text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool parse_trigger(const std::string& cell, CueTrigger& trigger, uint8_t& button_a,
                   uint8_t& button_b) {
    const std::string trimmed     = trim(cell);
    const std::string tap_suffix  = " tap";
    const std::string hold_suffix = " hold";
    const std::string arrow       = "→";

    if (ends_with(trimmed, tap_suffix)) {
        const std::string number = trim(trimmed.substr(0, trimmed.size() - tap_suffix.size()));
        if (!parse_u8(number, button_a)) {
            return false;
        }
        button_b = 0;
        trigger  = CueTrigger::Tap;
        return true;
    }

    if (ends_with(trimmed, hold_suffix)) {
        const std::string number = trim(trimmed.substr(0, trimmed.size() - hold_suffix.size()));
        if (!parse_u8(number, button_a)) {
            return false;
        }
        button_b = 0;
        trigger  = CueTrigger::Hold;
        return true;
    }

    const std::string::size_type arrow_pos = trimmed.find(arrow);
    if (arrow_pos == std::string::npos) {
        return false;
    }
    const std::string left  = trim(trimmed.substr(0, arrow_pos));
    const std::string right = trim(trimmed.substr(arrow_pos + arrow.size()));
    if (!parse_u8(left, button_a) || !parse_u8(right, button_b)) {
        return false;
    }
    if (button_a == button_b) {
        trigger = CueTrigger::SelfPair;
    } else {
        trigger = CueTrigger::PrefixSuffix;
    }
    return true;
}

bool is_markdown_separator(const std::string& cell) {
    if (cell.empty()) {
        return false;
    }
    return std::all_of(cell.begin(), cell.end(), [](char ch) { return ch == '-' || ch == ':'; });
}

std::vector<std::string> split_pipes(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    for (const char ch : line) {
        if (ch == '|') {
            cells.push_back(trim(current));
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    cells.push_back(trim(current));
    return cells;
}

const char* trigger_name(CueTrigger trigger) {
    switch (trigger) {
    case CueTrigger::Tap:
        return "tap";
    case CueTrigger::SelfPair:
        return "self-pair";
    case CueTrigger::PrefixSuffix:
        return "prefix+suffix";
    case CueTrigger::Hold:
        return "hold";
    }
    return "unknown";
}

bool parse_prd_table(const std::string& path, std::vector<ParsedCue>& rows) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "failed to open PRD at " << path << '\n';
        return false;
    }

    bool seen_heading = false;
    bool in_table     = false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!seen_heading) {
            if (line.find("### 6.2 Command table") != std::string::npos) {
                seen_heading = true;
            }
            continue;
        }

        if (!in_table) {
            if (line.empty() || line.front() != '|') {
                continue;
            }
            in_table = true;
        }

        if (line.empty() || line.front() != '|') {
            break;
        }

        const std::vector<std::string> cells = split_pipes(line);
        if (cells.size() < 4) {
            std::cerr << "malformed table row: " << line << '\n';
            return false;
        }

        const std::string& note_cell    = cells[1];
        const std::string& trigger_cell = cells[3];
        if (note_cell == "Note" || is_markdown_separator(note_cell)) {
            continue;
        }

        ParsedCue row{};
        if (!parse_u8(note_cell, row.note)) {
            std::cerr << "failed to parse note in row: " << line << '\n';
            return false;
        }
        if (!parse_trigger(trigger_cell, row.trigger, row.button_a, row.button_b)) {
            std::cerr << "failed to parse trigger '" << trigger_cell << "'\n";
            return false;
        }
        rows.push_back(row);
    }

    if (!seen_heading) {
        std::cerr << "did not find '### 6.2 Command table' in " << path << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    std::vector<ParsedCue> prd_rows;
    if (!parse_prd_table(PRD_PATH, prd_rows)) {
        return 1;
    }

    if (prd_rows.size() != 33U) {
        std::cerr << "PRD §6.2 has " << prd_rows.size() << " rows, expected 33\n";
        return 1;
    }
    if (std::size(CUE_TABLE) != 33U) {
        std::cerr << "CUE_TABLE has " << std::size(CUE_TABLE) << " entries, expected 33\n";
        return 1;
    }

    bool matched = true;
    for (std::size_t i = 0; i < 33U; ++i) {
        const ParsedCue& prd = prd_rows[i];
        const Cue& compiled  = CUE_TABLE[i];
        if (prd.note != compiled.note || prd.trigger != compiled.trigger ||
            prd.button_a != compiled.button_a || prd.button_b != compiled.button_b) {
            std::cerr << "row " << i << " mismatch:\n"
                      << "  PRD:      note=" << static_cast<unsigned>(prd.note)
                      << " trigger=" << trigger_name(prd.trigger)
                      << " buttons=" << static_cast<unsigned>(prd.button_a) << ','
                      << static_cast<unsigned>(prd.button_b) << '\n'
                      << "  CUE_TABLE: note=" << static_cast<unsigned>(compiled.note)
                      << " trigger=" << trigger_name(compiled.trigger)
                      << " buttons=" << static_cast<unsigned>(compiled.button_a) << ','
                      << static_cast<unsigned>(compiled.button_b) << '\n';
            matched = false;
        }
    }

    if (!matched) {
        return 1;
    }
    return 0;
}
