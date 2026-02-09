#pragma once

#include "MFCCExtractor.h"
#include "../Data/NoteData.h"
#include <vector>

class DTWAligner
{
public:
    struct AlignmentPoint
    {
        int referenceFrame;
        int targetFrame;
    };

    using AlignmentPath = std::vector<AlignmentPoint>;

    // Align target MFCC features to reference using DTW.
    // bandWidth limits the search area (Sakoe-Chiba band).
    // Returns the optimal alignment path.
    AlignmentPath align (const MFCCMatrix& reference,
                          const MFCCMatrix& target,
                          int bandWidth = 200);

    // Convert an alignment path to a continuous time-warp mapping.
    // Returns a vector where warp[targetFrame] = referenceTimeInSeconds.
    std::vector<double> pathToTimeWarp (const AlignmentPath& path,
                                         int numTargetFrames,
                                         double hopDuration);

    // Apply time-warp to notes by adjusting their timeOffset and durationScale.
    static void applyAlignmentToNotes (NoteList& targetNotes,
                                        const std::vector<double>& timeWarp,
                                        double hopDuration,
                                        int numTargetFrames);

private:
    static float euclideanDistance (const std::vector<float>& a,
                                     const std::vector<float>& b);
};
