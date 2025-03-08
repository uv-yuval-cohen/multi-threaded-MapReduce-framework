# MapReduce Framework

## 📌 Overview
This project implements a **multi-threaded MapReduce framework** in C++ using **pthreads** for parallel execution. It enables efficient processing of large data sets by breaking them into smaller chunks, performing a **map, shuffle, and reduce** operation in parallel.

---

## ✨ Features
✔️ Multi-threaded processing with **pthreads**  
✔️ Efficient **barrier synchronization** for phase transitions  
✔️ **Atomic counters** for progress tracking  
✔️ **Customizable client** implementation for different MapReduce tasks  
✔️ **Memory-safe design** with proper resource management  

---

## 📂 File Structure
📌 `Barrier.h` / `Barrier.cpp` - Implements a reusable barrier for thread synchronization.  
📌 `JobContext.h` - Manages job state, thread metadata, and synchronization mechanisms.  
📌 `MapReduceClient.h` - Defines the Map and Reduce function interfaces.  
📌 `MapReduceFramework.h` / `MapReduceFramework.cpp` - Implements the MapReduce pipeline (Map, Shuffle, Reduce).  
📌 `Makefile` - Builds the project into a static library `libMapReduceFramework.a`.  

---

## ⚙️ Installation and Compilation

1️⃣ Clone the repository:
   ```bash
   git clone <repository-url>
   cd <repository-name>
   ```

2️⃣ Compile the project using the provided Makefile:
   ```bash
   make
   ```
   This will generate the **libMapReduceFramework.a** static library.

---

## 🚀 Usage

To use the MapReduce framework, include `MapReduceFramework.h` and follow these steps:

### 🔹 Start a MapReduce Job
```cpp
JobHandle job = startMapReduceJob(client, inputVec, outputVec, numThreads);
```

### 🔹 Wait for Job Completion
```cpp
waitForJob(job);
```

### 🔹 Retrieve Job State
```cpp
JobState state;
getJobState(job, &state);
printf("Stage: %d, Progress: %.2f%%\n", state.stage, state.percentage);
```

### 🔹 Close the Job Handle
```cpp
closeJobHandle(job);
```

---

## 🏗️ Design Details

🔹 **Map Phase:**
   - Each thread processes a subset of the input vector.
   - The output is stored in **thread-specific intermediate vectors**.

🔹 **Sort Phase:**
   - Each thread sorts its intermediate vector independently.
   - The barrier ensures all threads complete before proceeding.

🔹 **Shuffle Phase:**
   - Performed by **Thread 0 only**.
   - Groups **identical keys** into vectors for the reduce phase.

🔹 **Reduce Phase:**
   - Each thread processes shuffled data.
   - Uses **emit3()** to store the final output.

---

## 🤝 Contribution
Contributions are welcome! Feel free to **submit issues** or **create pull requests** to improve this project. 😊

---

## 📜 License
This project is licensed under the **MIT License**.

