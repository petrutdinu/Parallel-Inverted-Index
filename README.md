# Assignment 1 - Inverted Index

## Overview

This project implements a **parallel inverted index computation** using the **Map-Reduce paradigm** with multithreading. The program distributes workload across **Mapper** and **Reducer** threads to efficiently process multiple text files and generate an inverted index.

## Main Function

The `main` function:

- Initializes resources and shared data structures
- Distributes files among **Mapper** and **Reducer** threads
- Ensures synchronization between threads

---

## Workflow

### 1. Argument Parsing

- Checks if the correct number of arguments is provided
- Reads the number of **Mapper** and **Reducer** threads
- Reads the path to the file containing the list of files to process

### 2. Reading Input Files

- Opens the specified file and reads the list of text files
- Stores file paths in a vector for further processing

### 3. Initializing Shared Data

- Populates a shared queue with file identifiers and paths
- Initializes a **barrier** for thread synchronization

### 4. Creating Threads

- Allocates `num_mappers + num_reducers` threads
- Initializes thread argument structures
- Uses `pthread_create` to start each thread

### 5. Waiting for Threads to Finish

- Uses `pthread_join` to wait for all threads to complete execution

### 6. Cleaning Up Resources

- Destroys allocated **mutexes** and **barriers**

---

## Map-Reduce Implementation

Each thread executes the **thread\_function**, behaving as either a **Mapper** or a **Reducer**, based on its assigned role.

### **Mapper Thread**

A **Mapper** thread follows these steps:

1. **Extracts a file** from the shared queue (with `queue.mutex` lock/unlock protection)
2. **Processes the file** by:
   - Cleaning and extracting words
   - Storing `{word, [File ID ...]}` pairs in a **local map**
   - Updating the **global** `word_map`, ensuring thread-safe access (`word_map.mutex`)
3. **Waits for all Mapper threads to finish** using a **barrier**

### **Reducer Thread**

A **Reducer** thread follows these steps:

1. **Ensures all Map operations are complete** using the **barrier**
2. **Processes assigned letters**:
   - Extracts words starting with the assigned letter (`word_map.mutex` lock/unlock)
   - Sorts words **by descending frequency** and then **alphabetically**
   - Writes results to an **output file specific to each letter**

---

## Output Structure

The generated inverted index is stored in **separate files**, each corresponding to a starting letter of words.

Example (`a.txt`):

```
apple: document1.txt, document3.txt
ant: document2.txt
```

---

## Summary

This project efficiently constructs an **inverted index** using **parallel Map-Reduce processing** with **multithreading**. It optimizes file parsing and data synchronization for improved performance.

### Key Features:

✅ **Parallel word indexing**\
✅ **Efficient thread synchronization**\
✅ **Lock-protected shared data structures**\
✅ **Multi-threaded Map-Reduce approach**

## Compilation and Execution

### **Compilation:**

Compile with `g++` and enable multithreading support:

```sh
g++ -Wall -Werror main.cpp -o tema1 -lpthread
```

### **Execution:**

```sh
./tema1 <num_mappers> <num_reducers> <file_list>
```