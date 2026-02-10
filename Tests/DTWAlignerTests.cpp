#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Alignment/DTWAligner.h"

using Catch::Matchers::WithinAbs;

// Helper: create a simple MFCC matrix with known values
static MFCCMatrix createSimpleMFCC (int numFrames, int numCoeffs, float baseValue = 0.0f)
{
    MFCCMatrix matrix;
    for (int i = 0; i < numFrames; ++i)
    {
        std::vector<float> frame (static_cast<size_t> (numCoeffs));
        for (int c = 0; c < numCoeffs; ++c)
            frame[static_cast<size_t> (c)] = baseValue + static_cast<float> (i) * 0.1f
                                              + static_cast<float> (c) * 0.01f;
        matrix.push_back (frame);
    }
    return matrix;
}

// ============================================================
// Basic alignment
// ============================================================
TEST_CASE ("DTWAligner returns empty for empty input", "[DTWAligner]")
{
    DTWAligner aligner;
    MFCCMatrix empty;
    auto reference = createSimpleMFCC (10, 13);

    auto path = aligner.align (empty, reference);
    REQUIRE (path.empty());

    path = aligner.align (reference, empty);
    REQUIRE (path.empty());
}

TEST_CASE ("DTWAligner aligns identical sequences to diagonal", "[DTWAligner]")
{
    DTWAligner aligner;
    auto matrix = createSimpleMFCC (20, 13);

    auto path = aligner.align (matrix, matrix);

    REQUIRE_FALSE (path.empty());

    // For identical sequences, the path should be close to diagonal
    for (const auto& point : path)
    {
        REQUIRE (std::abs (point.referenceFrame - point.targetFrame) <= 2);
    }
}

TEST_CASE ("DTWAligner path starts at (0,0)", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (15, 13, 0.0f);
    auto target = createSimpleMFCC (15, 13, 0.5f);

    auto path = aligner.align (ref, target);

    REQUIRE_FALSE (path.empty());
    REQUIRE (path.front().referenceFrame == 0);
    REQUIRE (path.front().targetFrame == 0);
}

TEST_CASE ("DTWAligner path ends at (N-1, M-1)", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (15, 13, 0.0f);
    auto target = createSimpleMFCC (20, 13, 0.5f);

    auto path = aligner.align (ref, target);

    REQUIRE_FALSE (path.empty());
    REQUIRE (path.back().referenceFrame == 14);
    REQUIRE (path.back().targetFrame == 19);
}

TEST_CASE ("DTWAligner handles different length sequences", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (10, 13);
    auto target = createSimpleMFCC (20, 13);

    auto path = aligner.align (ref, target);

    REQUIRE_FALSE (path.empty());
    REQUIRE (path.front().referenceFrame == 0);
    REQUIRE (path.front().targetFrame == 0);
    REQUIRE (path.back().referenceFrame == 9);
    REQUIRE (path.back().targetFrame == 19);
}

TEST_CASE ("DTWAligner path is monotonically non-decreasing", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (15, 13, 0.0f);
    auto target = createSimpleMFCC (20, 13, 0.3f);

    auto path = aligner.align (ref, target);

    REQUIRE (path.size() > 1);

    for (size_t i = 1; i < path.size(); ++i)
    {
        REQUIRE (path[i].referenceFrame >= path[i - 1].referenceFrame);
        REQUIRE (path[i].targetFrame >= path[i - 1].targetFrame);
    }
}

// ============================================================
// pathToTimeWarp
// ============================================================
TEST_CASE ("pathToTimeWarp produces correct length output", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (15, 13);
    auto target = createSimpleMFCC (20, 13);

    auto path = aligner.align (ref, target);
    double hopDuration = 1024.0 / 44100.0;

    auto warp = aligner.pathToTimeWarp (path, 20, hopDuration);

    REQUIRE (static_cast<int> (warp.size()) == 20);
}

TEST_CASE ("pathToTimeWarp is monotonically non-decreasing", "[DTWAligner]")
{
    DTWAligner aligner;
    auto ref = createSimpleMFCC (15, 13);
    auto target = createSimpleMFCC (20, 13);

    auto path = aligner.align (ref, target);
    double hopDuration = 1024.0 / 44100.0;

    auto warp = aligner.pathToTimeWarp (path, 20, hopDuration);

    for (size_t i = 1; i < warp.size(); ++i)
    {
        REQUIRE (warp[i] >= warp[i - 1] - 0.001);
    }
}

// ============================================================
// applyAlignmentToNotes
// ============================================================
TEST_CASE ("applyAlignmentToNotes adjusts note timing", "[DTWAligner]")
{
    // Create a simple time warp that shifts everything by 0.5 seconds
    double hopDuration = 1024.0 / 44100.0;
    int numFrames = 100;

    std::vector<double> timeWarp (static_cast<size_t> (numFrames));
    for (int i = 0; i < numFrames; ++i)
        timeWarp[static_cast<size_t> (i)] = (i * hopDuration) + 0.5; // offset by 0.5s

    NoteList notes;
    Note note;
    note.id = 1;
    note.startTime = 0.1;
    note.duration = 0.3;
    note.originalPitch = 440.0f;
    note.midiNote = 69;
    notes.push_back (note);

    DTWAligner::applyAlignmentToNotes (notes, timeWarp, hopDuration, numFrames);

    // Note should have a time offset applied
    REQUIRE (std::abs (notes[0].timeOffset) > 0.01);
}

TEST_CASE ("applyAlignmentToNotes clamps duration scale", "[DTWAligner]")
{
    double hopDuration = 1024.0 / 44100.0;
    int numFrames = 100;

    // Create a warp that compresses time drastically
    std::vector<double> timeWarp (static_cast<size_t> (numFrames));
    for (int i = 0; i < numFrames; ++i)
        timeWarp[static_cast<size_t> (i)] = i * hopDuration * 0.01; // very compressed

    NoteList notes;
    Note note;
    note.id = 1;
    note.startTime = 0.1;
    note.duration = 0.5;
    note.originalPitch = 440.0f;
    note.midiNote = 69;
    notes.push_back (note);

    DTWAligner::applyAlignmentToNotes (notes, timeWarp, hopDuration, numFrames);

    // Duration scale should be clamped to [0.25, 4.0]
    REQUIRE (notes[0].durationScale >= 0.25);
    REQUIRE (notes[0].durationScale <= 4.0);
}
