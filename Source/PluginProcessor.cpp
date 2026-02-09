#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Alignment/MFCCExtractor.h"
#include "Alignment/DTWAligner.h"

InKeyProcessor::InKeyProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    pitchAnalyzer.addListener (this);
}

InKeyProcessor::~InKeyProcessor()
{
    pitchAnalyzer.removeListener (this);
}

void InKeyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    audioCapture.prepare (sampleRate, samplesPerBlock);
}

void InKeyProcessor::releaseResources()
{
}

void InKeyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Capture audio when in CAPTURING state
    if (currentState.load() == PluginState::CAPTURING)
    {
        audioCapture.writeAudio (buffer, buffer.getNumSamples());
    }
    // Play back rendered audio when in PLAYING state
    else if (currentState.load() == PluginState::PLAYING)
    {
        buffer.clear();
        bool stillPlaying = audioEngine.fillBuffer (buffer, buffer.getNumSamples());

        if (! stillPlaying)
            currentState.store (PluginState::EDITING);
    }
}

juce::AudioProcessorEditor* InKeyProcessor::createEditor()
{
    return new InKeyEditor (*this);
}

void InKeyProcessor::startCapture()
{
    auto state = currentState.load();
    if (state != PluginState::IDLE && state != PluginState::EDITING)
        return;

    audioCapture.startCapture();
    currentState.store (PluginState::CAPTURING);
}

void InKeyProcessor::stopCapture()
{
    if (currentState.load() != PluginState::CAPTURING)
        return;

    audioCapture.stopCapture();

    currentState.store (PluginState::ANALYZING);
    pitchAnalyzer.startAnalysis (audioCapture.getCapturedAudio(),
                                  audioCapture.getSampleRate());
}

void InKeyProcessor::startPlayback (const NoteList& editedNotes)
{
    if (currentState.load() != PluginState::EDITING)
        return;

    if (activeTakeIndex < 0 || activeTakeIndex >= static_cast<int> (takes.size()))
        return;

    // Use the active take's audio for rendering
    auto& take = takes[static_cast<size_t> (activeTakeIndex)];
    auto rendered = audioEngine.render (take.audio, take.sampleRate, editedNotes);

    audioEngine.startPlayback (rendered, take.sampleRate);
    currentState.store (PluginState::PLAYING);
}

void InKeyProcessor::stopPlayback()
{
    if (currentState.load() != PluginState::PLAYING)
        return;

    audioEngine.stopPlayback();
    currentState.store (PluginState::EDITING);
}

void InKeyProcessor::analysisComplete()
{
    auto pitchResult = pitchAnalyzer.getResult();
    auto notes = noteSegmenter.segment (pitchResult);

    // Create a new take from the capture
    Take take;
    take.id = nextTakeId++;
    take.audio.makeCopyOf (audioCapture.getCapturedAudio());
    take.sampleRate = audioCapture.getSampleRate();
    take.pitchCurve = std::move (pitchResult);
    take.notes = std::move (notes);
    take.duration = audioCapture.getCapturedDurationSeconds();
    take.isReference = takes.empty(); // First take is reference

    takes.push_back (std::move (take));
    activeTakeIndex = static_cast<int> (takes.size()) - 1;

    if (takes.size() == 1)
        referenceTakeIndex = 0;

    currentState.store (PluginState::EDITING);
}

void InKeyProcessor::analysisProgress (float /*progress*/)
{
}

const NoteList& InKeyProcessor::getSegmentedNotes() const
{
    if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int> (takes.size()))
        return takes[static_cast<size_t> (activeTakeIndex)].notes;

    return emptyNotes;
}

void InKeyProcessor::saveActiveTakeNotes (const NoteList& editedNotes)
{
    if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int> (takes.size()))
        takes[static_cast<size_t> (activeTakeIndex)].notes = editedNotes;
}

const Take* InKeyProcessor::switchToTake (int index)
{
    if (index < 0 || index >= static_cast<int> (takes.size()))
        return nullptr;

    activeTakeIndex = index;
    return &takes[static_cast<size_t> (index)];
}

void InKeyProcessor::setReferenceTake (int index)
{
    if (index < 0 || index >= static_cast<int> (takes.size()))
        return;

    // Clear old reference flag
    for (auto& take : takes)
        take.isReference = false;

    takes[static_cast<size_t> (index)].isReference = true;
    referenceTakeIndex = index;
}

bool InKeyProcessor::alignActiveTakeToReference()
{
    if (activeTakeIndex < 0 || activeTakeIndex == referenceTakeIndex)
        return false;

    if (referenceTakeIndex < 0 || referenceTakeIndex >= static_cast<int> (takes.size()))
        return false;

    auto& refTake = takes[static_cast<size_t> (referenceTakeIndex)];
    auto& targetTake = takes[static_cast<size_t> (activeTakeIndex)];

    if (refTake.audio.getNumSamples() == 0 || targetTake.audio.getNumSamples() == 0)
        return false;

    // Extract MFCC features
    MFCCExtractor mfccExtractor;
    auto refMFCC = mfccExtractor.extract (refTake.audio.getReadPointer (0),
                                            refTake.audio.getNumSamples(),
                                            refTake.sampleRate);

    auto targetMFCC = mfccExtractor.extract (targetTake.audio.getReadPointer (0),
                                               targetTake.audio.getNumSamples(),
                                               targetTake.sampleRate);

    if (refMFCC.empty() || targetMFCC.empty())
        return false;

    // Run DTW alignment
    DTWAligner aligner;
    auto path = aligner.align (refMFCC, targetMFCC);

    if (path.empty())
        return false;

    // Convert to time warp
    double hopDuration = static_cast<double> (mfccExtractor.getSettings().hopSize) / targetTake.sampleRate;
    int numTargetFrames = static_cast<int> (targetMFCC.size());

    auto timeWarp = aligner.pathToTimeWarp (path, numTargetFrames, hopDuration);

    // Apply alignment to the target take's notes
    DTWAligner::applyAlignmentToNotes (targetTake.notes, timeWarp, hopDuration, numTargetFrames);
    targetTake.isAligned = true;

    return true;
}

void InKeyProcessor::getStateInformation (juce::MemoryBlock& /*destData*/)
{
}

void InKeyProcessor::setStateInformation (const void* /*data*/, int /*sizeInBytes*/)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InKeyProcessor();
}
