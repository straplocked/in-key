#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main (int argc, char* argv[])
{
    // Initialize JUCE MessageManager for Timer and async callback support
    juce::MessageManager::getInstance();

    int result = Catch::Session().run (argc, argv);

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();

    return result;
}
