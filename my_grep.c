/*
*   Written by Sergiy Momot.
*
*   Implementation of grep utility with basic search capability of substring
*       matching. It works on multiple files simultaneously by creating a thread
*       for each file the utility is searching.
*
*   CISC 3350 Workstation programming.
*   Brooklyn College of CUNY, Spring 2017.
*/

//glibc headers
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

//maximum supported length of a line in ASCII characters
#define MAX_LINE_LENGTH 1024

//shared read-only input data for created threads
//no need to protect since data is initialized before
//the first thread is created and is never modified after that
struct {
    char* pattern;
    char** files;
} input_data;

//thread function which searches an input file for occurrences of the pattern string
//prints matching lines and returns the number of total matches found in the file
//thread_id parameter is used for thread to know which file to search
void* threaded_search(void* thread_id);

//mutex variable for locking standart output file access
pthread_mutex_t mutex_stdout = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char** argv)
{
    //array of thread ids
    pthread_t* threads;

    //count of matches found in all input files
    unsigned long long total_matched_lines = 0;

    //number of files to process
    unsigned int files_count;

    //check if number of command-line arguments is correct
    if(argc < 3) {
        printf("Error: incorrect number of command line arguments.\n");
        printf("Usage: %s PATTERN [FILE]...\n", argv[0]);
        return EXIT_FAILURE;
    }

    //get the pattern string and check if it is not larger than MAX_LINE_LENGTH
    input_data.pattern = argv[1];
    if(strlen(input_data.pattern) > MAX_LINE_LENGTH) {
        printf("Error: pattern string is larger than max line length of %d.\n", MAX_LINE_LENGTH);
        return EXIT_FAILURE;
    }

    //all arguments except for program's name and PATTERN string are file paths
    files_count = argc - 2;

    //globally store paths of all input files
    input_data.files = (char**) calloc(files_count, sizeof(char*));
    for(int i = 0; i < files_count; i++)
        input_data.files[i] = argv[i + 2];

    //allocate memory to store ids of created threads
    //for each input file there will be exactly one thread which processes it
    //since threads will perform some IO operations, it makes sense to have
    //more threads than cores since threads will be blocked during IO requests
    threads = (pthread_t*) calloc(files_count, sizeof(pthread_t));

    //create threads and assign each one an id number
    //which is also an index of a file that this thread should process
    for(long i = 0; i < files_count; i++)
        if(pthread_create(&threads[i], NULL, threaded_search, (void*) i) > 0) {
            printf("Error: pthread could not start a new thread.\n");
            return EXIT_FAILURE;
        }

    //join every created thread and retrieve its return value
    for(int i = 0; i < files_count; i++) {
        long ret_value;
        pthread_join(threads[i], (void**) &ret_value);
        total_matched_lines += ret_value;
    }

    free(threads);

    //print the final result
    printf("Total matched lines: %llu\n", total_matched_lines);

    return EXIT_SUCCESS;
}

void* threaded_search(void* thread_id)
{
    //matches count
    long count = 0;

    //id of the current thread
    long id = (long) thread_id;

    //path to a file to be processed by the current thread
    char* filename = input_data.files[id];

    //open input file assigned to the current thread
    FILE* file = fopen(filename, "r");
    if(file == NULL) {
        pthread_mutex_lock(&mutex_stdout);
        printf("Thread %li could not open <%s> file for reading: %s\n", id, filename, strerror(errno));
        pthread_mutex_unlock(&mutex_stdout);
        return (void*) 0;
    }

    //read line by line and if there is a match then print the current line
    char line[MAX_LINE_LENGTH + 1];
    for(int ln = 1; fgets(line, MAX_LINE_LENGTH, file) != NULL; ln++)
        if(strstr(line, input_data.pattern) != NULL) {
            pthread_mutex_lock(&mutex_stdout);
            printf("<%s : %d>: %s", filename, ln, line);
            pthread_mutex_unlock(&mutex_stdout);
            count++;
        }

    fclose(file);

    //print the final result of the current thread
    pthread_mutex_lock(&mutex_stdout);
    printf("Thread %li: %li matches are found in file %s\n", id, count, filename);
    pthread_mutex_unlock(&mutex_stdout);

    return (void*) count;
}
