#include "DTWAligner.h"
#include <cmath>
#include <algorithm>
#include <limits>

float DTWAligner::euclideanDistance (const std::vector<float>& a,
                                      const std::vector<float>& b)
{
    float sum = 0.0f;
    size_t len = std::min (a.size(), b.size());
    for (size_t i = 0; i < len; ++i)
    {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt (sum);
}

DTWAligner::AlignmentPath DTWAligner::align (const MFCCMatrix& reference,
                                               const MFCCMatrix& target,
                                               int bandWidth)
{
    int N = static_cast<int> (reference.size());
    int M = static_cast<int> (target.size());

    if (N == 0 || M == 0)
        return {};

    constexpr float INF = std::numeric_limits<float>::infinity();

    // Banded DTW: for each row i, compute columns near the diagonal.
    // The diagonal maps (0,0) → (N-1, M-1), so for row i:
    //   j_center = i * (M-1) / (N-1)
    //   j_range = [j_center - bw, j_center + bw]

    // Store cost and parent per row, only within the band.
    // cost[i] has width = bandWidth*2+1 (or less at edges)
    // parent[i] stores direction: 0=diagonal, 1=vertical (from i-1,j), 2=horizontal (from i,j-1)

    struct BandRow
    {
        int lo = 0;
        int hi = 0;
        std::vector<float> cost;
        std::vector<int> parent;
    };

    std::vector<BandRow> rows (static_cast<size_t> (N));

    for (int i = 0; i < N; ++i)
    {
        int jCenter = (N > 1) ? static_cast<int> (static_cast<long long> (i) * (M - 1) / (N - 1)) : 0;
        int lo = std::max (0, jCenter - bandWidth);
        int hi = std::min (M - 1, jCenter + bandWidth);
        int width = hi - lo + 1;

        auto& row = rows[static_cast<size_t> (i)];
        row.lo = lo;
        row.hi = hi;
        row.cost.assign (static_cast<size_t> (width), INF);
        row.parent.assign (static_cast<size_t> (width), -1);

        for (int jj = 0; jj < width; ++jj)
        {
            int j = lo + jj;
            float dist = euclideanDistance (reference[static_cast<size_t> (i)],
                                            target[static_cast<size_t> (j)]);

            if (i == 0 && j == 0)
            {
                row.cost[static_cast<size_t> (jj)] = dist;
                row.parent[static_cast<size_t> (jj)] = 0;
                continue;
            }

            float minPrev = INF;
            int bestDir = -1;

            // Diagonal: from (i-1, j-1)
            if (i > 0 && j > 0)
            {
                auto& prevRow = rows[static_cast<size_t> (i - 1)];
                int prevIdx = (j - 1) - prevRow.lo;
                if (prevIdx >= 0 && prevIdx < static_cast<int> (prevRow.cost.size()))
                {
                    if (prevRow.cost[static_cast<size_t> (prevIdx)] < minPrev)
                    {
                        minPrev = prevRow.cost[static_cast<size_t> (prevIdx)];
                        bestDir = 0;
                    }
                }
            }

            // Vertical: from (i-1, j)
            if (i > 0)
            {
                auto& prevRow = rows[static_cast<size_t> (i - 1)];
                int prevIdx = j - prevRow.lo;
                if (prevIdx >= 0 && prevIdx < static_cast<int> (prevRow.cost.size()))
                {
                    if (prevRow.cost[static_cast<size_t> (prevIdx)] < minPrev)
                    {
                        minPrev = prevRow.cost[static_cast<size_t> (prevIdx)];
                        bestDir = 1;
                    }
                }
            }

            // Horizontal: from (i, j-1)
            if (jj > 0)
            {
                if (row.cost[static_cast<size_t> (jj - 1)] < minPrev)
                {
                    minPrev = row.cost[static_cast<size_t> (jj - 1)];
                    bestDir = 2;
                }
            }

            row.cost[static_cast<size_t> (jj)] = dist + minPrev;
            row.parent[static_cast<size_t> (jj)] = bestDir;
        }
    }

    // Backtrace from (N-1, M-1) or nearest reachable cell
    AlignmentPath path;
    int i = N - 1;
    int j = M - 1;

    // Clamp j to be within the last row's band
    j = std::max (rows[static_cast<size_t> (i)].lo,
                   std::min (j, rows[static_cast<size_t> (i)].hi));

    path.push_back ({ i, j });

    while (i > 0 || j > 0)
    {
        auto& row = rows[static_cast<size_t> (i)];
        int jj = j - row.lo;

        if (jj < 0 || jj >= static_cast<int> (row.parent.size()))
            break;

        int dir = row.parent[static_cast<size_t> (jj)];

        if (dir == 0) { --i; --j; }       // Diagonal
        else if (dir == 1) { --i; }        // Vertical
        else if (dir == 2) { --j; }        // Horizontal
        else break;                         // No valid parent

        path.push_back ({ i, j });
    }

    std::reverse (path.begin(), path.end());
    return path;
}

std::vector<double> DTWAligner::pathToTimeWarp (const AlignmentPath& path,
                                                  int numTargetFrames,
                                                  double hopDuration)
{
    std::vector<double> warp (static_cast<size_t> (numTargetFrames), 0.0);
    std::vector<int> counts (static_cast<size_t> (numTargetFrames), 0);

    for (const auto& p : path)
    {
        if (p.targetFrame >= 0 && p.targetFrame < numTargetFrames)
        {
            warp[static_cast<size_t> (p.targetFrame)] += p.referenceFrame * hopDuration;
            counts[static_cast<size_t> (p.targetFrame)]++;
        }
    }

    // Average multiple mappings per frame
    for (int i = 0; i < numTargetFrames; ++i)
    {
        if (counts[static_cast<size_t> (i)] > 0)
            warp[static_cast<size_t> (i)] /= counts[static_cast<size_t> (i)];
        else if (i > 0)
            warp[static_cast<size_t> (i)] = warp[static_cast<size_t> (i - 1)] + hopDuration;
    }

    return warp;
}

void DTWAligner::applyAlignmentToNotes (NoteList& targetNotes,
                                          const std::vector<double>& timeWarp,
                                          double hopDuration,
                                          int numTargetFrames)
{
    for (auto& note : targetNotes)
    {
        // Get frame indices for note start and end
        int startFrame = static_cast<int> (note.startTime / hopDuration);
        int endFrame = static_cast<int> (note.endTime() / hopDuration);
        startFrame = std::max (0, std::min (startFrame, numTargetFrames - 1));
        endFrame = std::max (0, std::min (endFrame, numTargetFrames - 1));

        double refStart = timeWarp[static_cast<size_t> (startFrame)];
        double refEnd = timeWarp[static_cast<size_t> (endFrame)];

        // Adjust note timing to match reference
        note.timeOffset = refStart - note.startTime;

        if (note.duration > 0.001)
        {
            double refDuration = refEnd - refStart;
            if (refDuration > 0.001)
                note.durationScale = std::max (0.25, std::min (4.0, refDuration / note.duration));
        }
    }
}
