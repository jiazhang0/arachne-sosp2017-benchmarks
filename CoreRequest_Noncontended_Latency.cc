#include <stdio.h>
#include <thread>
#include <atomic>

#include "CoreArbiterClient.h"
#include "Logger.h"
#include "Cycles.h"
#include "TimeTrace.h"
#include "Util.h"
#include "Stats.h"

using PerfUtils::TimeTrace;
using PerfUtils::Cycles;
using CoreArbiter::CoreArbiterClient;
using namespace CoreArbiter;

#define NUM_TRIALS 1000000

std::atomic<uint64_t> startCycles(0);
uint64_t arrayIndex = 0;
uint64_t latencies[NUM_TRIALS];

/**
  * This thread will get unblocked when a core is allocated, and will block
  * itself again when the number of cores is decreased.
  */
void coreExec(CoreArbiterClient& client) {
    for (int i = 0; i < NUM_TRIALS; i++) {
        client.blockUntilCoreAvailable();
        uint64_t endCycles = Cycles::rdtsc();
        latencies[arrayIndex++] = endCycles - startCycles;
        while (!client.mustReleaseCore());
    }
}

void coreRequest(CoreArbiterClient& client) {
    std::vector<uint32_t> oneCoreRequest = {1,0,0,0,0,0,0,0};

    client.setNumCores(oneCoreRequest);
    client.blockUntilCoreAvailable();

    std::vector<uint32_t> twoCoresRequest = {2,0,0,0,0,0,0,0};
    for (int i = 0; i < NUM_TRIALS; i++) {
        // When the number of blocked threads becomes nonzero, we request a core.
        while (client.getNumBlockedThreads() == 0);

        startCycles = Cycles::rdtsc();
        client.setNumCores(twoCoresRequest);
        // When the number of blocked threads becomes zero, we release a core.
        while (client.getNumBlockedThreads() == 1);
        client.setNumCores(oneCoreRequest);
    }
}

int main(){
    Logger::setLogLevel(ERROR);
    CoreArbiterClient& client =
        CoreArbiterClient::getInstance("/tmp/CoreArbiter/testsocket");
    std::thread requestThread(coreRequest, std::ref(client));
    while (client.getNumOwnedCores() == 0);

    std::thread coreThread(coreExec, std::ref(client));

    coreThread.join();
    requestThread.join();

    for (int i = 0; i < NUM_TRIALS; i++) {
        latencies[i] = Cycles::toNanoseconds(latencies[i]);
    }
    printStatistics("core_request_noncontended_latencies", latencies, NUM_TRIALS, "data");
}
