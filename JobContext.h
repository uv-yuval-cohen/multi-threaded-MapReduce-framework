//
// Created by Yuval Cohen on 10/07/2024.
//

#ifndef _JOBCONTEXT_H_
#define _JOBCONTEXT_H_

#include "MapReduceFramework.h"
#include "Barrier.h"
#include <pthread.h>
#include <atomic>
#include <vector>
#include <map>



struct JobContext {
    //vector of threads for thread management, order by their index.
    std::vector<pthread_t> threads;
    std::map<pthread_t, int> threadIdToIndex;  // Map thread IDs to their indices

    // Job state tracking using a single 64-bit atomic variable
    // 2 bits for stage, 31 bits for processed items, 31 bits for total items
    // Structure: +-----------------------------+-----------------------------+---------+
    //            |       Total Items (31 bits) | Processed Items (31 bits)   | Stage (2 bits) |
    std::atomic<uint64_t> jobStateAtomic;
    // Job state struct for easy access and updates
    JobState jobStateStruct;

    // Synchronization
    pthread_mutex_t mutex;
    pthread_mutex_t outputMutex; // mutex for emit3

    // Input, Intermediate, and Output Vectors
    InputVec inputVec;
    std::vector<IntermediateVec> intermediateVecs; // vec for each thread
    std::vector<IntermediateVec> shuffledQueue; // for shuffled phase
    OutputVec* outputVec;  // Pointer to the output vector

    // Barrier for phase synchronization
    Barrier* barrier;

    // Atomic counters for progress tracking
    std::atomic<int> mapCounter;
    std::atomic<int> intermediateCounter;
    std::atomic<int> shuffledCounter;
    std::atomic<int> reduceCounter;

    const MapReduceClient* client;  // Store the client reference

    bool jobCompleted;
    pthread_mutex_t waitMutex;
    pthread_cond_t waitCond;


    ~JobContext() {
      pthread_mutex_destroy(&mutex);
      pthread_mutex_destroy(&waitMutex);
      pthread_cond_destroy(&waitCond);
      pthread_mutex_destroy(&outputMutex);
      delete barrier;
    }
};

#endif //_JOBCONTEXT_H_
