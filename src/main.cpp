#include <pthread.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

using namespace std;

// Structură pentru gestionarea fișierelor
struct FileTask {
	string filename;
	int file_id;
};

// Structură pentru date partajate
struct SharedData {
	queue<FileTask> file_queue;
	map<string, set<int>> word_map;
	pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t word_map_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_barrier_t barrier;
};

// Structură pentru parametrii unui thread
struct ThreadArgs {
	int thread_id;
	int num_threads;
	int num_mappers;
	int num_reducers;
	SharedData* shared_data;
};

// Funcție pentru curățarea cuvintelor de caractere care nu sunt litere
void clean_word(string& word) {
	string cleaned;
	for (char c : word) {
		if (isalpha(c)) {
			cleaned += tolower(c);
		}
	}
	word = cleaned;
}

// Funcție Mapper-Reducer
void* thread_function(void* args) {
	ThreadArgs* targs = static_cast<ThreadArgs*>(args);
	int thread_id = targs->thread_id;
	SharedData& shared_data = *(targs->shared_data);

	// Mapper
	if (thread_id < targs->num_mappers) {
		while (true) {
			FileTask task;

			// Extrage un fișier din coadă
			pthread_mutex_lock(&shared_data.queue_mutex);
			if (!shared_data.file_queue.empty()) {
				task = shared_data.file_queue.front();
				shared_data.file_queue.pop();
			}
			pthread_mutex_unlock(&shared_data.queue_mutex);

			if (task.filename.empty()) break;  // Semnal de terminare

			// Procesare fișier
			ifstream input(task.filename);
			if (!input) {
				cerr << "Unable to open input file: " << task.filename << endl;
				continue;
			}

			map<string, set<int>> local_map;  // Map local pentru thread
			string word;
			while (input >> word) {
				clean_word(word);
				if (!word.empty()) {
					local_map[word].insert(task.file_id);
				}
			}
			input.close();

			// Actualizare word_map global
			pthread_mutex_lock(&shared_data.word_map_mutex);
			for (const auto& entry : local_map) {
				shared_data.word_map[entry.first].insert(entry.second.begin(), entry.second.end());
			}
			pthread_mutex_unlock(&shared_data.word_map_mutex);
		}

		// Așteaptă ca toate mapper ele să termine
		pthread_barrier_wait(&shared_data.barrier);
	// Reducer
	} else {
		pthread_barrier_wait(&shared_data.barrier);  // Așteaptă mapper ele

		// Procesează literele asignate acestui reducer
		int reducer_id = thread_id - targs->num_mappers;
		int total_reducers = targs->num_reducers;

		for (char c = 'a' + reducer_id; c <= 'z'; c += total_reducers) {
			vector<pair<string, set<int>>> words_for_letter;

			// Extrage datele relevante
			pthread_mutex_lock(&shared_data.word_map_mutex);
			for (const auto& entry : shared_data.word_map) {
				if (tolower(entry.first[0]) == c) {
					words_for_letter.emplace_back(entry.first, entry.second);
				}
			}
			pthread_mutex_unlock(&shared_data.word_map_mutex);

			// Sortare după dimensiunea setului (descrescător), apoi alfabetic
			sort(words_for_letter.begin(), words_for_letter.end(),
				 [](const pair<string, set<int>>& a, const pair<string, set<int>>& b) {
					 if (a.second.size() != b.second.size()) {
						 return a.second.size() > b.second.size();
					 }
					 return a.first < b.first;
				 });

			// Scriere în fișier
			ofstream output_file(string(1, c) + ".txt", ios::out);
			if (!output_file) {
				cerr << "Unable to create file for letter: " << c << endl;
				continue;
			}

			for (const auto& entry : words_for_letter) {
				output_file << entry.first << ":[";  // Scrie cuvântul
				for (auto it = entry.second.begin(); it != entry.second.end(); ++it) {
					if (it != entry.second.begin()) {
						output_file << " ";
					}
					output_file << *it;
				}
				output_file << "]\n";  // Termină lista
			}
			output_file.close();
		}
	}

	return nullptr;
}

int main(int argc, char* argv[]) {
	if (argc != 4) {
		cerr << "Usage: " << argv[0] << " <num_mapper_threads> <num_reducer_threads> <file_list>"
			 << endl;
		return 1;
	}

	int num_mappers = stoi(argv[1]);
	int num_reducers = stoi(argv[2]);
	string file_list = argv[3];
	int total_threads = num_mappers + num_reducers;

	// Citește lista de fișiere
	ifstream input_file(file_list);
	if (!input_file) {
		cerr << "Unable to open file list: " << file_list << endl;
		return 1;
	}

	int num_files;
	input_file >> num_files;  // Prima linie este numărul de fișiere

	vector<string> input_files;
	string file_path;
	while (getline(input_file, file_path)) {
		if (!file_path.empty()) {
			input_files.push_back(file_path);
		}
	}

	if (input_files.size() != static_cast<size_t>(num_files)) {
		cerr << "Mismatch between declared and actual number of files in " << file_list << endl;
		return 1;
	}

	// Inițializează structura de date partajate
	SharedData shared_data;
	for (size_t i = 0; i < input_files.size(); ++i) {
		shared_data.file_queue.push({input_files[i], static_cast<int>(i + 1)});
	}

	pthread_barrier_init(&shared_data.barrier, nullptr, total_threads);

	// Creează și pornește thread urile
	vector<pthread_t> threads(total_threads);
	vector<ThreadArgs> thread_args(total_threads);
	for (int i = 0; i < total_threads; ++i) {
		thread_args[i] = {i, total_threads, num_mappers, num_reducers, &shared_data};
		pthread_create(&threads[i], nullptr, thread_function, &thread_args[i]);
	}

	// Așteaptă thread urile să termine
	for (auto& thread : threads) {
		pthread_join(thread, nullptr);
	}

	// Curăță resursele
	pthread_mutex_destroy(&shared_data.queue_mutex);
	pthread_mutex_destroy(&shared_data.word_map_mutex);
	pthread_barrier_destroy(&shared_data.barrier);

	return 0;
}