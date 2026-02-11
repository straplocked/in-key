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
    renderDebounceTimer.startTimer (200);
}

InKeyProcessor::~InKeyProcessor()
{
    renderDebounceTimer.stopTimer();

    if (renderThread != nullptr)
    {
        renderThread->stopThread (2000);
        renderThread.reset();
    }

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

    auto state = currentState.load();

    // Capture audio when in CAPTURING state
    if (state == PluginState::CAPTURING)
    {
        // Record DAW position on the first block of capture
        if (captureJustStarted.exchange (false))
        {
            captureStartInSamples = 0;
            if (auto* playHead = getPlayHead())
            {
                if (auto pos = playHead->getPosition())
                {
                    if (auto samples = pos->getTimeInSamples())
                        captureStartInSamples = *samples;
                }
            }
        }

        audioCapture.writeAudio (buffer, buffer.getNumSamples());
    }
    // Manual playback (standalone / preview via Play button)
    else if (state == PluginState::PLAYING)
    {
        buffer.clear();
        bool stillPlaying = audioEngine.fillBuffer (buffer, buffer.getNumSamples());

        if (! stillPlaying)
            currentState.store (PluginState::EDITING);
    }
    // DAW transport sync: output rendered audio when DAW plays
    else if (state == PluginState::EDITING)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto pos = playHead->getPosition())
            {
                if (pos->getIsPlaying())
                {
                    if (auto timeInSamples = pos->getTimeInSamples())
                    {
                        int64_t dawPos = *timeInSamples;
                        int activeTake = activeTakeIndex;

                        if (activeTake >= 0 && activeTake < static_cast<int> (takes.size()))
                        {
                            auto& take = takes[static_cast<size_t> (activeTake)];
                            int64_t captStart = take.captureStartSampleInDAW;
                            double sr = take.sampleRate;

                            buffer.clear();

                            if (bypassEdits.load())
                            {
                                // A/B mode: play original unedited audio
                                int64_t offsetInCapture = dawPos - captStart;
                                if (offsetInCapture >= 0 && offsetInCapture < take.audio.getNumSamples())
                                {
                                    int available = juce::jmin (buffer.getNumSamples(),
                                        take.audio.getNumSamples() - static_cast<int> (offsetInCapture));
                                    const float* src = take.audio.getReadPointer (0, static_cast<int> (offsetInCapture));
                                    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                                        buffer.copyFrom (ch, 0, src, available);
                                }
                            }
                            else if (audioEngine.hasRenderedAudio.load())
                            {
                                audioEngine.fillBufferAtPosition (buffer, buffer.getNumSamples(),
                                                                  dawPos, captStart);
                            }

                            // Communicate playhead position to UI
                            double posInCapture = static_cast<double> (dawPos - captStart) / sr;
                            dawPlayheadInCapture.store (posInCapture);
                            dawIsPlaying.store (true);
                        }
                    }
                }
                else
                {
                    dawIsPlaying.store (false);
                    dawPlayheadInCapture.store (-1.0);
                }
            }
        }
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

    captureJustStarted.store (true);
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

    auto& take = takes[static_cast<size_t> (activeTakeIndex)];

    if (bypassEdits.load())
    {
        // A/B mode: play original unedited audio
        audioEngine.startPlayback (take.audio, take.sampleRate);
    }
    else
    {
        auto rendered = audioEngine.render (take.audio, take.sampleRate, editedNotes, take.pitchCurve);
        audioEngine.startPlayback (rendered, take.sampleRate);
    }

    currentState.store (PluginState::PLAYING);
}

void InKeyProcessor::stopPlayback()
{
    if (currentState.load() != PluginState::PLAYING)
        return;

    audioEngine.stopPlayback();
    currentState.store (PluginState::EDITING);
}

void InKeyProcessor::startPreviewPlayback (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    auto state = currentState.load();
    if (state != PluginState::EDITING)
        return;

    audioEngine.startPlayback (audio, sampleRate);
    currentState.store (PluginState::PLAYING);
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
    take.captureStartSampleInDAW = captureStartInSamples;

    takes.push_back (std::move (take));
    activeTakeIndex = static_cast<int> (takes.size()) - 1;

    if (takes.size() == 1)
        referenceTakeIndex = 0;

    currentState.store (PluginState::EDITING);

    // Auto-render so audio is ready for DAW playback
    requestRender();
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

void InKeyProcessor::requestRender()
{
    renderNeeded.store (true);
}

void InKeyProcessor::performRender()
{
    if (activeTakeIndex < 0 || activeTakeIndex >= static_cast<int> (takes.size()))
        return;

    auto& take = takes[static_cast<size_t> (activeTakeIndex)];

    // Copy notes and pitch curve for thread safety
    NoteList notesCopy = take.notes;
    PitchCurve curveCopy = take.pitchCurve;

    auto rendered = audioEngine.render (take.audio, take.sampleRate, notesCopy, curveCopy);
    audioEngine.swapRenderedBuffer (std::move (rendered));
}

void InKeyProcessor::RenderThread::run()
{
    do
    {
        processor.renderInProgress.store (true);
        processor.performRender();
        processor.renderInProgress.store (false);
    }
    while (processor.renderNeeded.exchange (false) && ! threadShouldExit());
}

void InKeyProcessor::RenderDebounceTimer::timerCallback()
{
    if (processor.renderNeeded.load() && ! processor.renderInProgress.load())
    {
        processor.renderNeeded.store (false);

        // Stop previous render thread if still around
        if (processor.renderThread != nullptr)
        {
            processor.renderThread->stopThread (2000);
            processor.renderThread.reset();
        }

        processor.renderThread = std::make_unique<RenderThread> (processor);
        processor.renderThread->startThread();
    }
}

void InKeyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Version 1 state format
    juce::ValueTree state ("InKeyState");
    state.setProperty ("version", 1, nullptr);
    state.setProperty ("activeTakeIndex", activeTakeIndex, nullptr);
    state.setProperty ("referenceTakeIndex", referenceTakeIndex, nullptr);
    state.setProperty ("nextTakeId", nextTakeId, nullptr);

    auto currentPluginState = currentState.load();
    state.setProperty ("pluginState",
        (currentPluginState == PluginState::EDITING || currentPluginState == PluginState::PLAYING)
            ? static_cast<int> (PluginState::EDITING) : static_cast<int> (PluginState::IDLE),
        nullptr);

    // Serialize each take
    for (size_t ti = 0; ti < takes.size(); ++ti)
    {
        const auto& take = takes[ti];
        juce::ValueTree takeTree ("Take");
        takeTree.setProperty ("id", take.id, nullptr);
        takeTree.setProperty ("sampleRate", take.sampleRate, nullptr);
        takeTree.setProperty ("duration", take.duration, nullptr);
        takeTree.setProperty ("isReference", take.isReference, nullptr);
        takeTree.setProperty ("isAligned", take.isAligned, nullptr);
        takeTree.setProperty ("captureStartSample", static_cast<juce::int64> (take.captureStartSampleInDAW), nullptr);

        // Audio data as Base64
        if (take.audio.getNumSamples() > 0)
        {
            int numSamples = take.audio.getNumSamples();
            takeTree.setProperty ("audioNumSamples", numSamples, nullptr);
            juce::MemoryBlock audioBlock (static_cast<size_t> (numSamples) * sizeof (float));
            std::memcpy (audioBlock.getData(), take.audio.getReadPointer (0),
                          static_cast<size_t> (numSamples) * sizeof (float));
            takeTree.setProperty ("audioData", audioBlock.toBase64Encoding(), nullptr);
        }

        // Pitch curve
        juce::ValueTree pitchTree ("PitchCurve");
        for (const auto& frame : take.pitchCurve)
        {
            juce::ValueTree frameTree ("Frame");
            frameTree.setProperty ("time", frame.time, nullptr);
            frameTree.setProperty ("freq", static_cast<double> (frame.frequency), nullptr);
            frameTree.setProperty ("conf", static_cast<double> (frame.confidence), nullptr);
            pitchTree.appendChild (frameTree, nullptr);
        }
        takeTree.appendChild (pitchTree, nullptr);

        // Notes
        juce::ValueTree notesTree ("Notes");
        for (const auto& note : take.notes)
        {
            juce::ValueTree noteTree ("Note");
            noteTree.setProperty ("id", note.id, nullptr);
            noteTree.setProperty ("startTime", note.startTime, nullptr);
            noteTree.setProperty ("duration", note.duration, nullptr);
            noteTree.setProperty ("originalPitch", static_cast<double> (note.originalPitch), nullptr);
            noteTree.setProperty ("midiNote", note.midiNote, nullptr);
            noteTree.setProperty ("confidence", static_cast<double> (note.confidence), nullptr);
            noteTree.setProperty ("pitchShift", static_cast<double> (note.pitchShiftSemitones), nullptr);
            noteTree.setProperty ("formantShift", static_cast<double> (note.formantShiftSemitones), nullptr);
            noteTree.setProperty ("timeOffset", note.timeOffset, nullptr);
            noteTree.setProperty ("durationScale", note.durationScale, nullptr);
            noteTree.setProperty ("gainDB", static_cast<double> (note.gainDB), nullptr);
            noteTree.setProperty ("pitchDriftScale", static_cast<double> (note.pitchDriftScale), nullptr);
            noteTree.setProperty ("isMuted", note.isMuted, nullptr);
            noteTree.setProperty ("transitionInDuration", static_cast<double> (note.transitionInDuration), nullptr);
            noteTree.setProperty ("transitionInShape", static_cast<double> (note.transitionInShape), nullptr);

            // Serialize pitch envelope control points
            if (! note.pitchEnvelope.empty())
            {
                juce::ValueTree envTree ("PitchEnvelope");
                for (const auto& cp : note.pitchEnvelope)
                {
                    juce::ValueTree cpTree ("ControlPoint");
                    cpTree.setProperty ("time", cp.timeNormalized, nullptr);
                    cpTree.setProperty ("pitch", static_cast<double> (cp.pitchOffsetSemitones), nullptr);
                    envTree.appendChild (cpTree, nullptr);
                }
                noteTree.appendChild (envTree, nullptr);
            }

            notesTree.appendChild (noteTree, nullptr);
        }
        takeTree.appendChild (notesTree, nullptr);

        state.appendChild (takeTree, nullptr);
    }

    // Write the ValueTree to the memory block
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void InKeyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));

    if (! state.isValid() || state.getType().toString() != "InKeyState")
        return;

    int version = state.getProperty ("version", 0);
    if (version < 1)
        return;

    takes.clear();
    activeTakeIndex = state.getProperty ("activeTakeIndex", -1);
    referenceTakeIndex = state.getProperty ("referenceTakeIndex", 0);
    nextTakeId = state.getProperty ("nextTakeId", 1);

    // Restore takes
    for (int ti = 0; ti < state.getNumChildren(); ++ti)
    {
        auto takeTree = state.getChild (ti);
        if (takeTree.getType().toString() != "Take")
            continue;

        Take take;
        take.id = takeTree.getProperty ("id", 0);
        take.sampleRate = takeTree.getProperty ("sampleRate", 44100.0);
        take.duration = takeTree.getProperty ("duration", 0.0);
        take.isReference = takeTree.getProperty ("isReference", false);
        take.isAligned = takeTree.getProperty ("isAligned", false);
        take.captureStartSampleInDAW = static_cast<int64_t> ((juce::int64) takeTree.getProperty ("captureStartSample", 0));

        // Restore audio
        int numSamples = takeTree.getProperty ("audioNumSamples", 0);
        if (numSamples > 0)
        {
            juce::String base64 = takeTree.getProperty ("audioData", "");
            if (base64.isNotEmpty())
            {
                juce::MemoryBlock audioBlock;
                audioBlock.fromBase64Encoding (base64);

                if (audioBlock.getSize() == static_cast<size_t> (numSamples) * sizeof (float))
                {
                    take.audio.setSize (1, numSamples);
                    std::memcpy (take.audio.getWritePointer (0), audioBlock.getData(),
                                  static_cast<size_t> (numSamples) * sizeof (float));
                }
            }
        }

        // Restore pitch curve
        auto pitchTree = takeTree.getChildWithName ("PitchCurve");
        if (pitchTree.isValid())
        {
            for (int fi = 0; fi < pitchTree.getNumChildren(); ++fi)
            {
                auto frameTree = pitchTree.getChild (fi);
                PitchFrame frame;
                frame.time = frameTree.getProperty ("time", 0.0);
                frame.frequency = static_cast<float> ((double) frameTree.getProperty ("freq", 0.0));
                frame.confidence = static_cast<float> ((double) frameTree.getProperty ("conf", 0.0));
                take.pitchCurve.push_back (frame);
            }
        }

        // Restore notes
        auto notesTree = takeTree.getChildWithName ("Notes");
        if (notesTree.isValid())
        {
            for (int ni = 0; ni < notesTree.getNumChildren(); ++ni)
            {
                auto noteTree = notesTree.getChild (ni);
                Note note;
                note.id = noteTree.getProperty ("id", 0);
                note.startTime = noteTree.getProperty ("startTime", 0.0);
                note.duration = noteTree.getProperty ("duration", 0.0);
                note.originalPitch = static_cast<float> ((double) noteTree.getProperty ("originalPitch", 0.0));
                note.midiNote = noteTree.getProperty ("midiNote", 0);
                note.confidence = static_cast<float> ((double) noteTree.getProperty ("confidence", 0.0));
                note.pitchShiftSemitones = static_cast<float> ((double) noteTree.getProperty ("pitchShift", 0.0));
                note.formantShiftSemitones = static_cast<float> ((double) noteTree.getProperty ("formantShift", 0.0));
                note.timeOffset = noteTree.getProperty ("timeOffset", 0.0);
                note.durationScale = noteTree.getProperty ("durationScale", 1.0);
                note.gainDB = static_cast<float> ((double) noteTree.getProperty ("gainDB", 0.0));
                note.pitchDriftScale = static_cast<float> ((double) noteTree.getProperty ("pitchDriftScale", 1.0));
                note.isMuted = noteTree.getProperty ("isMuted", false);
                note.transitionInDuration = static_cast<float> ((double) noteTree.getProperty ("transitionInDuration", 0.0));
                note.transitionInShape = static_cast<float> ((double) noteTree.getProperty ("transitionInShape", 0.0));

                // Restore pitch envelope
                auto envTree = noteTree.getChildWithName ("PitchEnvelope");
                if (envTree.isValid())
                {
                    for (int ci = 0; ci < envTree.getNumChildren(); ++ci)
                    {
                        auto cpTree = envTree.getChild (ci);
                        PitchControlPoint cp;
                        cp.timeNormalized = cpTree.getProperty ("time", 0.0);
                        cp.pitchOffsetSemitones = static_cast<float> ((double) cpTree.getProperty ("pitch", 0.0));
                        note.pitchEnvelope.push_back (cp);
                    }
                }

                take.notes.push_back (note);
            }
        }

        takes.push_back (std::move (take));
    }

    // Restore plugin state
    int savedState = state.getProperty ("pluginState", static_cast<int> (PluginState::IDLE));
    if (savedState == static_cast<int> (PluginState::EDITING) && ! takes.empty())
    {
        currentState.store (PluginState::EDITING);
        requestRender();  // Pre-render so DAW playback works immediately
    }
    else
    {
        currentState.store (PluginState::IDLE);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InKeyProcessor();
}
