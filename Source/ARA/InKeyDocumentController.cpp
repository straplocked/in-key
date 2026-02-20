// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 InKey contributors
// This file is part of InKey. See LICENSE for details.

#include "InKeyDocumentController.h"
#include "InKeyPlaybackRenderer.h"
#include "InKeyEditorRenderer.h"
#include "../Analysis/SibilantDetector.h"
#include "../Analysis/VibratoAnalyzer.h"

// Global ARA factory entry point required by JUCE
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation
        ::createARAFactory<InKeyDocumentController>();
}

juce::ARAPlaybackRenderer* InKeyDocumentController::doCreatePlaybackRenderer()
{
    return new InKeyPlaybackRenderer (getDocumentController());
}

juce::ARAEditorRenderer* InKeyDocumentController::doCreateEditorRenderer()
{
    return new InKeyEditorRenderer (getDocumentController());
}

juce::ARAEditorView* InKeyDocumentController::doCreateEditorView()
{
    return new juce::ARAEditorView (getDocumentController());
}

void InKeyDocumentController::addListener (Listener* l)
{
    listeners.add (l);
}

void InKeyDocumentController::removeListener (Listener* l)
{
    listeners.remove (l);
}

void InKeyDocumentController::didEnableAudioSourceSamplesAccess (
    juce::ARAAudioSource* source, bool enable) noexcept
{
    juce::ARADocumentControllerSpecialisation::didEnableAudioSourceSamplesAccess (source, enable);

    if (enable)
        analyzeSource (source);
}

void InKeyDocumentController::analyzeSource (juce::ARAAudioSource* source)
{
    if (source == nullptr || ! source->isSampleAccessEnabled())
        return;

    const auto numSamples = source->getSampleCount();
    const auto sampleRate = source->getSampleRate();
    const auto numChannels = source->getChannelCount();

    if (numSamples == 0 || sampleRate <= 0.0)
        return;

    // Read audio from host using ARA's HostAudioReader
    ARA::PlugIn::HostAudioReader hostReader (source);

    juce::AudioBuffer<float> audioBuffer (1, static_cast<int> (numSamples));

    if (numChannels == 1)
    {
        // Mono: read directly
        float* buffers[1] = { audioBuffer.getWritePointer (0) };
        hostReader.readAudioSamples (0, numSamples, reinterpret_cast<void* const*> (buffers));
    }
    else
    {
        // Multi-channel: read all channels and average to mono
        juce::AudioBuffer<float> fullBuffer (static_cast<int> (numChannels),
                                              static_cast<int> (numSamples));

        std::vector<float*> buffers (static_cast<size_t> (numChannels));
        for (int ch = 0; ch < static_cast<int> (numChannels); ++ch)
            buffers[static_cast<size_t> (ch)] = fullBuffer.getWritePointer (ch);

        hostReader.readAudioSamples (0, numSamples, reinterpret_cast<void* const*> (buffers.data()));

        // Mixdown to mono
        audioBuffer.clear();
        float gain = 1.0f / static_cast<float> (numChannels);
        for (int ch = 0; ch < static_cast<int> (numChannels); ++ch)
            audioBuffer.addFrom (0, 0, fullBuffer, ch, 0, static_cast<int> (numSamples), gain);
    }

    // Create or retrieve analysis entry
    auto persistentID = std::string (source->getPersistentID());
    if (analysisMap.find (persistentID) == analysisMap.end())
        analysisMap[persistentID] = std::make_unique<ARASourceAnalysis>();

    auto* analysis = analysisMap[persistentID].get();
    analysis->sampleRate = sampleRate;
    analysis->numSamples = numSamples;
    analysis->analysisComplete.store (false);

    // Store audio temporarily for sibilant detection after pitch analysis
    analysisAudioBuffer = std::move (audioBuffer);

    // Run analysis on background thread
    currentlyAnalyzingSource = source;
    pitchAnalyzer.addListener (this);
    pitchAnalyzer.startAnalysis (analysisAudioBuffer, sampleRate);
}

void InKeyDocumentController::analysisComplete()
{
    if (currentlyAnalyzingSource == nullptr)
        return;

    auto persistentID = std::string (currentlyAnalyzingSource->getPersistentID());
    auto it = analysisMap.find (persistentID);
    if (it == analysisMap.end())
        return;

    auto* analysis = it->second.get();

    // Segment notes from pitch curve
    analysis->pitchCurve = pitchAnalyzer.getResult();
    analysis->notes = noteSegmenter.segment (analysis->pitchCurve);

    // Run sibilant detection
    if (analysisAudioBuffer.getNumSamples() > 0)
    {
        SibilantDetector sibilantDetector;
        sibilantDetector.detectForNotes (analysis->notes,
                                          analysisAudioBuffer.getReadPointer (0),
                                          analysisAudioBuffer.getNumSamples(),
                                          analysis->sampleRate);
    }

    // Run vibrato analysis
    for (auto& note : analysis->notes)
    {
        auto vibratoInfo = VibratoAnalyzer::analyzeNote (analysis->pitchCurve,
                                                          note.startTime, note.endTime());
        note.vibratoRate = vibratoInfo.vibratoRate;
    }

    analysis->analysisComplete.store (true);

    // Clear the temporary audio buffer
    analysisAudioBuffer = juce::AudioBuffer<float>();

    auto* sourcePtr = currentlyAnalyzingSource;
    currentlyAnalyzingSource = nullptr;
    pitchAnalyzer.removeListener (this);

    juce::MessageManager::callAsync ([this, sourcePtr]()
    {
        listeners.call ([sourcePtr] (Listener& l) {
            l.araAnalysisComplete (sourcePtr);
        });
    });
}

void InKeyDocumentController::analysisProgress (float progress)
{
    if (currentlyAnalyzingSource == nullptr)
        return;

    auto persistentID = std::string (currentlyAnalyzingSource->getPersistentID());
    if (auto it = analysisMap.find (persistentID); it != analysisMap.end())
        it->second->analysisProgress.store (progress);

    auto* sourcePtr = currentlyAnalyzingSource;
    juce::MessageManager::callAsync ([this, sourcePtr, progress]()
    {
        listeners.call ([sourcePtr, progress] (Listener& l) {
            l.araAnalysisProgress (sourcePtr, progress);
        });
    });
}

ARASourceAnalysis* InKeyDocumentController::getAnalysis (juce::ARAAudioSource* source)
{
    if (source == nullptr)
        return nullptr;

    auto persistentID = std::string (source->getPersistentID());
    auto it = analysisMap.find (persistentID);
    return (it != analysisMap.end()) ? it->second.get() : nullptr;
}

bool InKeyDocumentController::doStoreObjectsToStream (
    juce::ARAOutputStream& output,
    const juce::ARAStoreObjectsFilter* filter)
{
    // Serialize analysis results and note edits to a ValueTree,
    // then write the bytes through the ARA archive writer
    juce::ValueTree state ("InKeyARAState");
    state.setProperty ("version", 1, nullptr);

    for (const auto& [persistentID, analysis] : analysisMap)
    {
        if (! analysis->analysisComplete.load())
            continue;

        juce::ValueTree sourceTree ("Source");
        sourceTree.setProperty ("persistentID", juce::String (persistentID), nullptr);
        sourceTree.setProperty ("sampleRate", analysis->sampleRate, nullptr);
        sourceTree.setProperty ("numSamples", static_cast<juce::int64> (analysis->numSamples), nullptr);

        // Serialize notes with their edit state
        juce::ValueTree notesTree ("Notes");
        for (const auto& note : analysis->notes)
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
            noteTree.setProperty ("vibratoDepthScale", static_cast<double> (note.vibratoDepthScale), nullptr);
            noteTree.setProperty ("driftScale", static_cast<double> (note.driftScale), nullptr);
            noteTree.setProperty ("vibratoRate", static_cast<double> (note.vibratoRate), nullptr);
            noteTree.setProperty ("fadeInMs", static_cast<double> (note.fadeInMs), nullptr);
            noteTree.setProperty ("fadeOutMs", static_cast<double> (note.fadeOutMs), nullptr);
            noteTree.setProperty ("sibilantGainDB", static_cast<double> (note.sibilantGainDB), nullptr);
            noteTree.setProperty ("transitionInDuration", static_cast<double> (note.transitionInDuration), nullptr);
            noteTree.setProperty ("transitionInShape", static_cast<double> (note.transitionInShape), nullptr);

            // Serialize sibilant regions
            if (! note.sibilantRegions.empty())
            {
                juce::ValueTree sibTree ("SibilantRegions");
                for (const auto& region : note.sibilantRegions)
                {
                    juce::ValueTree regionTree ("Region");
                    regionTree.setProperty ("start", region.first, nullptr);
                    regionTree.setProperty ("end", region.second, nullptr);
                    sibTree.appendChild (regionTree, nullptr);
                }
                noteTree.appendChild (sibTree, nullptr);
            }

            // Serialize pitch envelope
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
        sourceTree.appendChild (notesTree, nullptr);

        // Serialize pitch curve
        juce::ValueTree pitchTree ("PitchCurve");
        for (const auto& frame : analysis->pitchCurve)
        {
            juce::ValueTree frameTree ("Frame");
            frameTree.setProperty ("time", frame.time, nullptr);
            frameTree.setProperty ("freq", static_cast<double> (frame.frequency), nullptr);
            frameTree.setProperty ("conf", static_cast<double> (frame.confidence), nullptr);
            pitchTree.appendChild (frameTree, nullptr);
        }
        sourceTree.appendChild (pitchTree, nullptr);

        state.appendChild (sourceTree, nullptr);
    }

    // Serialize ValueTree to bytes
    juce::MemoryBlock block;
    {
        juce::MemoryOutputStream stream (block, false);
        state.writeToStream (stream);
    }

    return output.write (block.getData(), block.getSize());
}

bool InKeyDocumentController::doRestoreObjectsFromStream (
    juce::ARAInputStream& input,
    const juce::ARARestoreObjectsFilter* filter)
{
    // Read all bytes from the ARA archive
    juce::MemoryBlock block;
    const auto archiveSize = input.getTotalLength();
    if (archiveSize <= 0)
        return true; // Empty archive is valid

    block.setSize (static_cast<size_t> (archiveSize));
    auto bytesRead = input.read (block.getData(), static_cast<int> (archiveSize));
    if (bytesRead != static_cast<int> (archiveSize))
        return false;

    auto state = juce::ValueTree::readFromData (block.getData(), block.getSize());
    if (! state.isValid() || state.getType().toString() != "InKeyARAState")
        return false;

    int version = state.getProperty ("version", 0);
    if (version < 1)
        return false;

    // Restore per-source analysis data and note edits
    for (int si = 0; si < state.getNumChildren(); ++si)
    {
        auto sourceTree = state.getChild (si);
        if (sourceTree.getType().toString() != "Source")
            continue;

        auto persistentID = sourceTree.getProperty ("persistentID", "").toString().toStdString();
        if (persistentID.empty())
            continue;

        auto analysis = std::make_unique<ARASourceAnalysis>();
        analysis->sampleRate = sourceTree.getProperty ("sampleRate", 44100.0);
        analysis->numSamples = static_cast<int64_t> ((juce::int64) sourceTree.getProperty ("numSamples", 0));

        // Restore pitch curve
        auto pitchTree = sourceTree.getChildWithName ("PitchCurve");
        if (pitchTree.isValid())
        {
            for (int fi = 0; fi < pitchTree.getNumChildren(); ++fi)
            {
                auto frameTree = pitchTree.getChild (fi);
                PitchFrame frame;
                frame.time = frameTree.getProperty ("time", 0.0);
                frame.frequency = static_cast<float> ((double) frameTree.getProperty ("freq", 0.0));
                frame.confidence = static_cast<float> ((double) frameTree.getProperty ("conf", 0.0));
                analysis->pitchCurve.push_back (frame);
            }
        }

        // Restore notes
        auto notesTree = sourceTree.getChildWithName ("Notes");
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
                note.vibratoDepthScale = static_cast<float> ((double) noteTree.getProperty ("vibratoDepthScale", 1.0));
                note.driftScale = static_cast<float> ((double) noteTree.getProperty ("driftScale", 1.0));
                note.vibratoRate = static_cast<float> ((double) noteTree.getProperty ("vibratoRate", 0.0));
                note.fadeInMs = static_cast<float> ((double) noteTree.getProperty ("fadeInMs", 0.0));
                note.fadeOutMs = static_cast<float> ((double) noteTree.getProperty ("fadeOutMs", 0.0));
                note.sibilantGainDB = static_cast<float> ((double) noteTree.getProperty ("sibilantGainDB", 0.0));
                note.transitionInDuration = static_cast<float> ((double) noteTree.getProperty ("transitionInDuration", 0.0));
                note.transitionInShape = static_cast<float> ((double) noteTree.getProperty ("transitionInShape", 0.0));

                // Restore sibilant regions
                auto sibTree = noteTree.getChildWithName ("SibilantRegions");
                if (sibTree.isValid())
                {
                    for (int ri = 0; ri < sibTree.getNumChildren(); ++ri)
                    {
                        auto regionTree = sibTree.getChild (ri);
                        double start = regionTree.getProperty ("start", 0.0);
                        double end = regionTree.getProperty ("end", 0.0);
                        note.sibilantRegions.push_back ({ start, end });
                    }
                }

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

                analysis->notes.push_back (note);
            }
        }

        analysis->analysisComplete.store (true);
        analysisMap[persistentID] = std::move (analysis);
    }

    return true;
}
