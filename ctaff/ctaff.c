#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/kissfft/kiss_fftr.h"

#define PI 3.1415927
#define FRAME_SIZE 2048
#define FRAME_TIME 0.04644

typedef struct BeatNode {
    struct BeatNode *next;
    // struct BeatNode *prev;
    float time;
    char layer;
} BeatNode_t;

typedef struct BeatList {
    struct BeatNode *head, *tail;
} BeatList_t;

typedef struct BassVariables {
    double last_averages[2]; // {n-1, n-2}
    double last_maximums[2];
    double last_increases[8];
    double last_frequencies[8];
    double last_total_increases[2];

    bool last_was_detected[2];
} BassVariables_t;

const char kPathSeparator[2] =
#ifdef _WIN32
                               "\\";
#else
                               "/";
#endif

void add_beat(BeatList_t *beats, float time, char layer) {
    BeatNode_t *new_node = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    new_node->time = time;
    new_node->layer = layer;
    
    beats->tail->next = new_node;
    beats->tail = new_node;
}

bool detect_beat(double average, double maximum, double increase, int increase_count, double average_increase_ratios[2], double maximum_increase_ratios[2], BassVariables_t *bass_variables) {
    // TODO : average > some preprocess of whole song, or relative
    // TODO : lower average and maximum requirements and raise relative requirements
    // maybe lower last maximums requirement with higher increases
    if (average > 75 && maximum > 150 && increase > 200 && increase / bass_variables->last_maximums[0] > 1.5 && increase > bass_variables->last_total_increases[0] * .7)
        return true;

    return false;

}

void detect_bass(double *out, BeatList_t *beats, float time, BassVariables_t *bass_variables) {
    bool detected = false;
    double average = 1, increase = 1, maximum = 1, maximum_average = 0;
    int increase_count = 0, maximum_index = 0;

    float start_time = 336.0, end_time = 339.4;

    for (int i = 1; i <= 8; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - bass_variables->last_frequencies[i - 1];
        double last_increase = bass_variables->last_increases[i - 1];
        double max_increase = (freq_increase > last_increase ? freq_increase : last_increase);
        if (max_increase > 75) {
            increase += max_increase;
            increase_count++;
        }

        average += magnitude;
        if (magnitude > maximum) {
            maximum = magnitude;
            maximum_index = i;
        }
        bass_variables->last_frequencies[i - 1] = magnitude;
        bass_variables->last_increases[i - 1] = freq_increase;
    }

    maximum_average = (out[maximum_index + (maximum_index != 8 && out[maximum_index + 1] > out[maximum_index - 1] ? 1 : -1)] + maximum) / 2;
    average /= 8;

    double average_increase_ratios[2] = {average / bass_variables->last_averages[0], average / bass_variables->last_averages[1]},
           maximum_increase_ratios[2] = {maximum_average / bass_variables->last_maximums[0], maximum_average / bass_variables->last_maximums[1]};
           // increase_differences[2]    = {increase - bass_variables->last_increases[0], increase - bass_variables->last_increases[1]};

    // DEBUG
    if (time >= start_time && time <= end_time) {
        printf("******************* %f seconds *******************\n", time);
        for (int i = 1; i <= 8; i++) {
            double magnitude = out[i];
            printf("%f | %f | %f\n", i*44100/2048., magnitude, bass_variables->last_increases[i - 1]);
        }
        printf("last: %d, lastlast: %d\n", bass_variables->last_was_detected[0], bass_variables->last_was_detected[1]);
        printf("MAXIMUM: %f\nAVERAGE: %f\nINCREASE: %f\nINCREASE COUNT: %d\nINCREASE PROP: %f\n", maximum_average, average, increase, increase_count, increase / bass_variables->last_maximums[0]);
    }
    
    if (bass_variables->last_was_detected[0] && increase > 250 && increase > 1.3 * bass_variables->last_total_increases[0] && beats->tail->layer != 'C' &&
        (increase / bass_variables->last_averages[0] > bass_variables->last_total_increases[0] / bass_variables->last_averages[1] || 
         increase / bass_variables->last_maximums[0] > bass_variables->last_total_increases[0] / bass_variables->last_maximums[1])) {
        
        beats->tail->time = (beats->tail->time + time) / 2;
        beats->tail->layer = 'C';
        detected = true;
    }
    else if (detect_beat(average, maximum_average, increase, increase_count, average_increase_ratios, maximum_increase_ratios, bass_variables)) {
        if (!bass_variables->last_was_detected[0] && !bass_variables->last_was_detected[1]) {
            add_beat(beats, time, 'A');
            detected = true;
            // printf("DETECTED\n");
        }
        else if (!bass_variables->last_was_detected[0] && bass_variables->last_was_detected[1] && average > 100 && 
                 (increase / bass_variables->last_maximums[0] > bass_variables->last_total_increases[0] / bass_variables->last_maximums[1] ||
                  increase / bass_variables->last_maximums[0] > 5)) {

            add_beat(beats, time + FRAME_TIME / 2, 'B');
            detected = true;
            // printf("DETECTED\n");
        }
        // else {
        //     average /= 1.5;
        //     maximum_average /= 1.5;
        //     increase /= 1.5;
        // }
    }

    bass_variables->last_averages[1] = bass_variables->last_averages[0];
    bass_variables->last_averages[0] = average;
    bass_variables->last_maximums[1] = bass_variables->last_maximums[0];
    bass_variables->last_maximums[0] = maximum_average;
    // bass_variables->last_increases[1] = bass_variables->last_increases[0];
    // bass_variables->last_increases[0] = (increase > average ? (increase + average) / 2 : increase);
    bass_variables->last_total_increases[1] = bass_variables->last_total_increases[0];
    bass_variables->last_total_increases[0] = increase;

    bass_variables->last_was_detected[1] = bass_variables->last_was_detected[0];
    if (detected) {
        bass_variables->last_was_detected[0] = true;
    }
    else {
        bass_variables->last_was_detected[0] = false;
    }
}

int main(int argc, char *argv[]) {
    system("mkdir -p tmp");
    FILE *label_ptr = fopen("tmp/_labels.txt", "w+");
    FILE *file_ptr = NULL;

    if (argc > 1) {
        int opt;
        while ((opt = getopt(argc, argv, "i:o:")) != -1) {
            switch (opt) {
                case 'i':
                    {
                        int buffer_size = 70+251; // 70 chars for command + 251 chars for filepath

                        char command_buffer[buffer_size]; 
                        int make_command = snprintf(command_buffer, buffer_size, "ffmpeg -i \"%s\" -loglevel error -y -ac 1 -f f32le -ar 44100 -", optarg);
                        if (make_command < 0 || make_command >= buffer_size) {
                            printf("Error: Can't decode filename. Ensure the file path is at most 250 characters long.\n");
                            return 1;
                        }
                        file_ptr = popen(command_buffer, "r");
                    }
                    break;
                case 'o':
                    label_ptr = fopen(optarg, "w+");
                    if (label_ptr == NULL) {
                        printf("Error: output file can't be opened. Defaulting to tmp/_labels.txt\n");
                        label_ptr = fopen("tmp/_labels.txt", "w+");
                    }
                    break;
            }
        }
    }
    else {
        printf("Error: no input file specified.\n");
        return 1;
    }

    // ensure decoding created output
    int c = fgetc(file_ptr);
    if (c == EOF) {
        printf("Error: couldn't open the input file.\n");
        return 1;
    }
    else
        ungetc(c, file_ptr);
    
    float sample;

    kiss_fft_scalar in[FRAME_SIZE];
    kiss_fft_cpx out[FRAME_SIZE];
    kiss_fftr_cfg fwd = kiss_fftr_alloc(FRAME_SIZE, 0, NULL, NULL);

    BeatNode_t *start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    start->next = NULL;

    BeatList_t *beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    beats->head = start;
    beats->tail = start;

    BassVariables_t *bass_variables = (BassVariables_t*) malloc(sizeof(BassVariables_t));
    memset(bass_variables->last_averages, 1, sizeof(bass_variables->last_averages));
    memset(bass_variables->last_maximums, 1, sizeof(bass_variables->last_maximums));
    memset(bass_variables->last_increases, 1, sizeof(bass_variables->last_increases));
    memset(bass_variables->last_was_detected, false, sizeof(bass_variables->last_was_detected));
    memset(bass_variables->last_frequencies, 10, sizeof(bass_variables->last_frequencies));
    memset(bass_variables->last_total_increases, 1, sizeof(bass_variables->last_total_increases));

    int frame = 0;
    double magnitudes[FRAME_SIZE / 2];

    while (1) {
        for (int i = 0; i < FRAME_SIZE; i++) {
            // hanning window weight
            float window_weight = powf(sinf(PI * i / FRAME_SIZE), 2);
            fread((void*)(&sample), sizeof(sample), 1, file_ptr);
            in[i] = (kiss_fft_scalar)(sample * window_weight);
        }

        if (feof(file_ptr)) 
            break;

        kiss_fftr(fwd, &in[0], &out[0]);

        // for (int i = 0; i <= FRAME_SIZE / 2; i++) // full spectrum
        for (int i = 0; i <= 8; i++) // only frequencies needed for now
            magnitudes[i] = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);

        float time = (frame + 0.5) * FRAME_SIZE / 44100.;
        detect_bass(&magnitudes[0], beats, time, bass_variables);

        frame++;
    }
    for (BeatNode_t *current_node = beats->head->next; current_node != NULL; current_node = current_node->next) {
        fprintf(label_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);
    }
    for (BeatNode_t *current_node = beats->head; current_node != NULL; current_node = current_node->next) {
        free(current_node);
    }
    free(beats);
    kiss_fftr_free(fwd);

    return 0;
}