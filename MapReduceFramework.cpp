#include "MapReduceFramework.h"
#include "Barrier.h"
#include <pthread.h>
#include <vector>
#include "JobContext.h"
#include <iostream>
#include <algorithm>


//declarations
void
initializeJobContext (JobContext *jobContext, const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel);
void *threadFunction (void *arg);
void mapPhase(JobContext* jobContext, int threadIndex);
void sortPhase(JobContext* jobContext, int threadIndex);
bool compareKeys(K2* a, K2* b);
void shufflePhase(JobContext* jobContext);
void reducePhase(JobContext* jobContext, int threadIndex);
inline uint64_t encodeJobState(stage_t stage, uint32_t processed, uint32_t
total);
inline void decodeJobState(uint64_t encodedState, stage_t &stage, uint32_t
&processed, uint32_t &total);
void updateJobState(JobContext* jobContext, uint32_t increment);

JobHandle startMapReduceJob (const MapReduceClient &client,
                             const InputVec &inputVec, OutputVec &outputVec,
                             int multiThreadLevel)
{
  JobContext *jobContext = new JobContext;
  initializeJobContext (jobContext, client, inputVec, multiThreadLevel);

  // Set the output vector pointer
  jobContext->outputVec = &outputVec;

  // Create threads
  for (int i = 0; i < multiThreadLevel; ++i)
  {
    pthread_t thread;
    if (pthread_create (&thread, nullptr, threadFunction, jobContext) != 0)
    {
      fprintf (stderr, "system error: pthread_create\n");
      exit (1);
    }
    jobContext->threads.push_back (thread);
    jobContext->threadIdToIndex[thread] = i;
  }

  return static_cast<JobHandle>(jobContext);

}

void waitForJob(JobHandle job)
{
  JobContext *jobContext = static_cast<JobContext *>(job);
  if (pthread_mutex_lock(&jobContext->waitMutex) != 0)
  {
    fprintf(stderr, "system error: pthread_mutex_lock\n");
    exit(1);
  }
  while (!jobContext->jobCompleted)
  {
    for (auto &thread : jobContext->threads)
    {
      if (pthread_join(thread, nullptr) != 0)
      {
        fprintf(stderr, "system error: pthread_join\n");
        exit(1);
      }
    }
    jobContext->jobCompleted = true;
    if (pthread_cond_broadcast(&jobContext->waitCond) != 0)
    {
      fprintf(stderr, "system error: pthread_cond_broadcast\n");
      exit(1);
    }
  }
  if (pthread_mutex_unlock(&jobContext->waitMutex) != 0)
  {
    fprintf(stderr, "system error: pthread_mutex_unlock\n");
    exit(1);
  }
}

void getJobState(JobHandle job, JobState* state)
{
  JobContext *jobContext = static_cast<JobContext *>(job);
  uint64_t encodedState = jobContext->jobStateAtomic.load();

  // Decode
  stage_t stage;
  uint32_t processedItems, totalItems;
  decodeJobState(encodedState, stage, processedItems, totalItems);

  // Update the state
  state->stage = stage;
  state->percentage = (totalItems == 0) ? 0.0f : (100.0f * processedItems / totalItems);
}


void closeJobHandle(JobHandle job)
{
  waitForJob(job); // Ensure all threads have completed

  JobContext *jobContext = static_cast<JobContext *>(job);

  // Destroy mutex and condition variable used in jobContext
  if (pthread_mutex_destroy(&jobContext->mutex) != 0) {
    fprintf(stderr, "system error: pthread_mutex_destroy\n");
    exit(1);
  }
  if (pthread_mutex_destroy(&jobContext->waitMutex) != 0) {
    fprintf(stderr, "system error: pthread_mutex_destroy\n");
    exit(1);
  }
  if (pthread_mutex_destroy(&jobContext->outputMutex) != 0) {
    fprintf(stderr, "system error: pthread_mutex_destroy\n");
    exit(1);
  }
  if (pthread_cond_destroy(&jobContext->waitCond) != 0) {
    fprintf(stderr, "system error: pthread_cond_destroy\n");
    exit(1);
  }

  delete jobContext;
}


void emit2 (K2 *key, V2 *value, void *context)
{
  JobContext *jobContext = static_cast<JobContext *>(context);
  int threadIndex = jobContext->threadIdToIndex[pthread_self()];
  jobContext->intermediateVecs[threadIndex].emplace_back(key, value);
  jobContext->intermediateCounter.fetch_add(1);

}

void emit3(K3 *key, V3 *value, void *context) {
  JobContext *jobContext = static_cast<JobContext *>(context);
  if (pthread_mutex_lock(&jobContext->outputMutex) != 0) {
    fprintf(stderr, "system error: pthread_mutex_lock\n");
    exit(1);
  }
  jobContext->outputVec->emplace_back(key, value);
  if (pthread_mutex_unlock(&jobContext->outputMutex) != 0) {
    fprintf(stderr, "system error: pthread_mutex_unlock\n");
    exit(1);
  }
  // Increment reduceCounter
  jobContext->reduceCounter.fetch_add(1);
  // Update job state
  updateJobState(jobContext, 1);
}

void
initializeJobContext (JobContext *jobContext, const MapReduceClient &client, const InputVec &inputVec, int multiThreadLevel)
{
  // Initialize atomic variables
  // Initial state - stage UNDEFINED, processed 0, total 0
  jobContext->jobStateAtomic.store(encodeJobState(UNDEFINED_STAGE, 0, inputVec.size()));
  jobContext->mapCounter.store (0);
  jobContext->intermediateCounter.store(0);
  jobContext->shuffledCounter.store(0);
  jobContext->reduceCounter.store (0);

  // Initialize mutex and condition variable with error handling
  if (pthread_mutex_init (&jobContext->mutex, nullptr) != 0)
  {
    fprintf (stderr, "system error: pthread_mutex_init\n");
    exit (1);
  }
  if (pthread_mutex_init (&jobContext->waitMutex, nullptr) != 0)
  {
    fprintf (stderr, "system error: pthread_cond_init\n");
    exit (1);
  }

  // Initialize client
  jobContext->client = &client;

  // Initialize vectors
  jobContext->inputVec = inputVec;
  jobContext->intermediateVecs.resize (multiThreadLevel);


  // Initialize barrier
  jobContext->barrier = new Barrier (multiThreadLevel);

  jobContext->jobCompleted = false;
  if (pthread_mutex_init(&jobContext->waitMutex, nullptr) != 0)
  {
    fprintf(stderr, "system error: pthread_mutex_init\n");
    exit(1);
  }
  if (pthread_cond_init(&jobContext->waitCond, nullptr) != 0)
  {
    fprintf(stderr, "system error: pthread_cond_init\n");
    exit(1);
  }
  if (pthread_mutex_init(&jobContext->outputMutex, nullptr) != 0) {
    fprintf(stderr, "system error: pthread_mutex_init\n");
    exit(1);
  }

}

void *threadFunction(void *arg) {
  JobContext *jobContext = static_cast<JobContext *>(arg);
  int threadIndex = jobContext->threadIdToIndex[pthread_self()];

  // Set the job state to MAP_STAGE
  if (threadIndex == 0) {
    jobContext->jobStateAtomic.store(encodeJobState(MAP_STAGE, 0, jobContext->inputVec.size()));
  }

  // Map Phase
  mapPhase(jobContext, threadIndex);

  // Sort Phase
  sortPhase(jobContext, threadIndex);

  // Barrier - Wait for all threads to finish the sort phase
  jobContext->barrier->barrier();

  // Shuffle Phase (only thread 0)
  if (threadIndex == 0) {
    uint32_t totalIntermediateItems = jobContext->intermediateCounter.load(); // Use the
    // atomic counter
    jobContext->jobStateAtomic.store(encodeJobState(SHUFFLE_STAGE, 0, totalIntermediateItems));
    shufflePhase(jobContext);
    uint32_t totalShuffledItems = jobContext->shuffledCounter.load(); // Use the atomic counter
    jobContext->jobStateAtomic.store(encodeJobState(REDUCE_STAGE, 0, totalShuffledItems));
  }

  // Barrier - wait for thread 0 to finish shuffle
  jobContext->barrier->barrier();


  // Reduce Phase
  reducePhase(jobContext, threadIndex);

  return nullptr;
}

void mapPhase(JobContext* jobContext, int threadIndex) {
  size_t index;
  while ((index = jobContext->mapCounter.fetch_add(1)) <
  jobContext->inputVec.size()) {
    updateJobState(jobContext, 1);
    const InputPair& inputPair = jobContext->inputVec[index];
    jobContext->client->map(inputPair.first, inputPair.second, jobContext);
  }
}

void sortPhase(JobContext* jobContext, int threadIndex)
{
  IntermediateVec &intermediateVec = jobContext->intermediateVecs[threadIndex];
  std::sort (intermediateVec.begin (), intermediateVec.end (),
             [] (const IntermediatePair &a, const IntermediatePair &b)
             {
                 return *a.first < *b.first;
             });
}

// Shuffle Phase Function
void shufflePhase(JobContext* jobContext)
{
  std::vector<IntermediateVec> &intermediateVecs = jobContext->intermediateVecs;
  std::vector<IntermediateVec>& shuffledQueue = jobContext->shuffledQueue;
  std::atomic<int>& shuffledCounter = jobContext->shuffledCounter;

  while (true)
  {
    bool found = false;
    K2 *currentKey = nullptr;

    // Find the largest key among the back elements of all intermediate vectors
    for (const auto &vec: intermediateVecs)
    {
      if (!vec.empty ())
      {
        if (currentKey == nullptr
            || compareKeys (currentKey, vec.back ().first))
        {
          currentKey = vec.back ().first;
        }
        found = true;
      }
    }

    // Break the loop if all vectors are empty
    if (!found)
    {
      break;
    }

    // Create a new sequence for the current largest key
    IntermediateVec newVec;
    for (auto &vec: intermediateVecs)
    {
      while (!vec.empty () && !compareKeys (currentKey, vec.back ().first)
             && !compareKeys (vec.back ().first, currentKey))
      {
        newVec.push_back (vec.back ());
        vec.pop_back ();
        // Update job state
        updateJobState(jobContext, 1);
      }
    }
    // Insert the new sequence into the shuffled queue and update the counter
    shuffledQueue.push_back(newVec);
    shuffledCounter.fetch_add(1);


  }

}

// Comparison function for K2* keys
bool compareKeys(K2* a, K2* b) {
  return *a < *b;
}

void reducePhase(JobContext* jobContext, int threadIndex) {
  while (true) {
    IntermediateVec currentVec;

    // Pop a vector from the back of the shuffled vectors
    pthread_mutex_lock(&jobContext->mutex);
    if (!jobContext->shuffledQueue.empty()) {
      currentVec = std::move(jobContext->shuffledQueue.back());
      jobContext->shuffledQueue.pop_back();
    }
    pthread_mutex_unlock(&jobContext->mutex);

    if (currentVec.empty()) {
      break;
    }

    // Run the reduce function on the current vector
    jobContext->client->reduce(&currentVec, jobContext);
  }
}


//Bit Manipulations!
inline uint64_t encodeJobState(stage_t stage, uint32_t processed, uint32_t total) {
  return (static_cast<uint64_t>(stage) << 62) | (static_cast<uint64_t>(total) << 31) | static_cast<uint64_t>(processed);
}

inline void decodeJobState(uint64_t encodedState, stage_t &stage, uint32_t &processed, uint32_t &total) {
  stage = static_cast<stage_t>((encodedState >> 62) & 0x3);
  total = (encodedState >> 31) & 0x7FFFFFFF;
  processed = encodedState & 0x7FFFFFFF;
}

void updateJobState(JobContext* jobContext, uint32_t increment) {
  uint64_t oldState = jobContext->jobStateAtomic.load();
  stage_t stage;
  uint32_t processed, total;
  decodeJobState(oldState, stage, processed, total);
  jobContext->jobStateAtomic.store(encodeJobState(stage, processed + increment, total));
}