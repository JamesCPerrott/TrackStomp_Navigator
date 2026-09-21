#include "config.h"
#include "harness.h"

#include <cstdint>

// Pure lookup: a transmitted note maps back to the buttons that produced it.
int main() {
    const CueOrigin intro = cue_origin_for_note(0);
    REQUIRE(intro.kind == CueClass::Pair);
    REQUIRE(intro.button_a == 1);
    REQUIRE(intro.button_b == 6);

    const CueOrigin outro = cue_origin_for_note(19);
    REQUIRE(outro.kind == CueClass::Pair);
    REQUIRE(outro.button_a == 5);
    REQUIRE(outro.button_b == 9);

    const CueOrigin pad = cue_origin_for_note(24);
    REQUIRE(pad.kind == CueClass::SelfPair);
    REQUIRE(pad.button_a == 3);
    REQUIRE(pad.button_b == 3);

    const CueOrigin repeat = cue_origin_for_note(20);
    REQUIRE(repeat.kind == CueClass::Standalone);
    REQUIRE(repeat.button_a == 10);
    REQUIRE(repeat.button_b == 0);

    const CueOrigin loop = cue_origin_for_note(21);
    REQUIRE(loop.kind == CueClass::Hold);
    REQUIRE(loop.button_a == 10);
    REQUIRE(loop.button_b == 0);

    const CueOrigin section_minus = cue_origin_for_note(27);
    REQUIRE(section_minus.kind == CueClass::Hold);
    REQUIRE(section_minus.button_a == 1);
    REQUIRE(section_minus.button_b == 0);

    const CueOrigin mute = cue_origin_for_note(32);
    REQUIRE(mute.kind == CueClass::Hold);
    REQUIRE(mute.button_a == 8);
    REQUIRE(mute.button_b == 0);

    const CueOrigin unmapped = cue_origin_for_note(33);
    REQUIRE(unmapped.kind == CueClass::Invalid);
    REQUIRE(unmapped.button_a == uint8_t{0xFF});
    REQUIRE(unmapped.button_b == uint8_t{0xFF});

    for (const Cue& cue : CUE_TABLE) {
        const CueOrigin origin = cue_origin_for_note(cue.note);
        REQUIRE(origin.kind != CueClass::Invalid);
        REQUIRE(origin.button_a == cue.button_a);
        REQUIRE(origin.button_b == cue.button_b);
    }

    return 0;
}
