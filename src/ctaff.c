#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/kissfft/kiss_fftr.h"

#define PI 3.1415927
#define FRAME_SIZE 2048
#define FRAME_TIME 0.04644 // 2048 / 44100

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
    int last_maximum_index;

    bool last_was_detected[2];
} BassVariables_t;

typedef struct MidrangeVariables {
    double last_increases[80];
    double last_frequencies[80];

    bool last_was_detected[2];
} MidrangeVariables_t;

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

bool detect_bass_beat(double running_average, double average, double maximum, double increase, int increase_count, double average_increase_ratios[2], double maximum_increase_ratios[2], BassVariables_t *bass_variables) {
    if (running_average >= 100) {
        if (((average > 70 && increase > 350) || average > 80) && maximum > 155 && increase > 250 && increase / bass_variables->last_maximums[0] > 1.4 && increase > bass_variables->last_total_increases[0] * .6)
            return true;
    }
    else if (running_average >= 80) {
        if (((average > 65 && increase > 350) || average > 75) && maximum > 145 && increase > 225 && increase / bass_variables->last_maximums[0] > 1.5 && increase > bass_variables->last_total_increases[0] * .7)
            return true;
    }
    else if (running_average >= 60) {
        if (((average > 60 && increase > 350) || average > 70) && maximum > 130 && increase > 200 && increase / bass_variables->last_maximums[0] > 1.5 && increase > bass_variables->last_total_increases[0] * .7)
            return true;
    }
    else if (running_average >= 50) {
        if (((average > 50 && increase > 300) || average > 65) && maximum > 120 && increase > 200 && increase / bass_variables->last_maximums[0] > 1.5 && increase > bass_variables->last_total_increases[0] * .7)
            return true;
    }
    else if (running_average >= 40) {
        if (((average > 45 && increase > 250) || average > 60) && maximum > 100 && increase > 150 && increase / bass_variables->last_maximums[0] > 1.5 && increase > bass_variables->last_total_increases[0] * .8)
            return true;
    }
    else {
        if (((average > 40 && increase > 200) || average > 50) && maximum > 75 && increase > 100 && increase / bass_variables->last_maximums[0] > 1.6 && increase > bass_variables->last_total_increases[0] * .9)
            return true;
    }

    return false;
}

bool detect_midrange_beat(double running_average, double maximum_average, double increase) {
    if (running_average > 15) {
        if (maximum_average > 75 && increase > 275)
            return true;
    }
    else if (running_average > 10) {
        if (maximum_average > 65 && increase > 225)
            return true;
    }
    else if (running_average > 5) {
        if (maximum_average > 55 && increase > 150) {
            return true;
        }
    }
    else if (maximum_average > 45 && increase > 100) {
        return true;
    }
    return false;
}

void detect_bass(double *out, BeatList_t *beats, float time, double average, double running_average, BassVariables_t *bass_variables) {
    bool detected = false;
    double increase = 1, maximum_average = 1;
    int increase_count = 0, maximum_index = 0;

    for (int i = 1; i <= 8; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - bass_variables->last_frequencies[i - 1];
        double last_increase = bass_variables->last_increases[i - 1];
        double max_increase = freq_increase > last_increase ? freq_increase : last_increase;
        if (max_increase > 30 + running_average / 2) { // can raise to reduce false positives
            increase += max_increase;
            increase_count++;
        }
        double magnitude_average = (out[i + 1] + magnitude) / 2;
        if (magnitude_average > maximum_average && i != 8) {
            maximum_average = magnitude_average;
            maximum_index = i;
        }
        bass_variables->last_frequencies[i - 1] = magnitude;
        bass_variables->last_increases[i - 1] = freq_increase;
    }

    double average_increase_ratios[2] = {average / bass_variables->last_averages[0], average / bass_variables->last_averages[1]},
           maximum_increase_ratios[2] = {maximum_average / bass_variables->last_maximums[0], maximum_average / bass_variables->last_maximums[1]};
    
    if (bass_variables->last_was_detected[0] && increase > 250 && increase > 1.3 * bass_variables->last_total_increases[0] && beats->tail->layer != 'C' &&
        (increase / bass_variables->last_averages[0] > bass_variables->last_total_increases[0] / bass_variables->last_averages[1] || 
         increase / bass_variables->last_maximums[0] > bass_variables->last_total_increases[0] / bass_variables->last_maximums[1])) {
        
        if (average > bass_variables->last_averages[0] * 1.1)
            beats->tail->time += FRAME_TIME / 2;
        detected = true;
    }
    else if (!bass_variables->last_was_detected[0] && detect_bass_beat(running_average, average, maximum_average, increase, increase_count, average_increase_ratios, maximum_increase_ratios, bass_variables)) {
        if (!bass_variables->last_was_detected[1]) {
            if (bass_variables->last_maximum_index > maximum_index && bass_variables->last_maximums[0] > running_average / 2) {
                time -= FRAME_TIME / 2;
                maximum_index = bass_variables->last_maximum_index;
            }

            char layer = 'A' + maximum_index / 5;
            if (time - beats->tail->time < .15 && beats->tail->layer == layer)
                layer = layer == 'B' ? 'A' : 'B';
            add_beat(beats, time, layer);
            detected = true;
        }
        else if (average > 100 && (increase / bass_variables->last_maximums[0] > bass_variables->last_total_increases[0] / bass_variables->last_maximums[1] ||
                                   increase / bass_variables->last_maximums[0] > 5)) {
            if (time - beats->tail->time >= FRAME_TIME * 2.5 && bass_variables->last_maximum_index > maximum_index && bass_variables->last_maximums[0] > running_average / 2) {
                time -= FRAME_TIME / 2;
                maximum_index = bass_variables->last_maximum_index;
            }

            char layer = 'A' + maximum_index / 5;
            if (time - beats->tail->time < .15 && beats->tail->layer == layer)
                layer = layer == 'B' ? 'A' : 'B';
            add_beat(beats, time, layer);
            detected = true;
        }
    }

    bass_variables->last_averages[1] = bass_variables->last_averages[0];
    bass_variables->last_averages[0] = average;
    bass_variables->last_maximums[1] = bass_variables->last_maximums[0];
    bass_variables->last_maximums[0] = maximum_average;
    bass_variables->last_total_increases[1] = bass_variables->last_total_increases[0];
    bass_variables->last_total_increases[0] = increase;
    bass_variables->last_maximum_index = maximum_index;

    bass_variables->last_was_detected[1] = bass_variables->last_was_detected[0];
    
    if (detected)
        bass_variables->last_was_detected[0] = true;
    else
        bass_variables->last_was_detected[0] = false;
}

void detect_midrange(double *out, BeatList_t *beats, float time, double average, double running_average, MidrangeVariables_t *midrange_variables) {
    double increase = 1, maximum_average = 1;
    int increase_count = 0, maximum_index = 0;

    for (int i = 10; i <= 89; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - midrange_variables->last_frequencies[i - 10];
        double last_increase = midrange_variables->last_increases[i - 10];
        double max_increase = freq_increase > last_increase ? freq_increase : last_increase;
        if (max_increase > running_average * 2) {
            increase += max_increase;
            increase_count++;
        }
        double magnitude_average = (out[i + 1] + magnitude) / 2;
        if (magnitude_average > maximum_average && i != 89) {
            maximum_average = magnitude_average;
            maximum_index = i;
        }
        midrange_variables->last_frequencies[i - 10] = magnitude;
    }

    if (!midrange_variables->last_was_detected[0] && !midrange_variables->last_was_detected[1] && detect_midrange_beat(running_average, maximum_average, increase)) {
        char layer = 'C' + (maximum_index > 15 ? 2 : 0);
        if (time - beats->tail->time <= .2 && beats->tail->layer != 'D')
            layer = 'D';
        add_beat(beats, time, layer);
        midrange_variables->last_was_detected[0] = true;
    }
    else {
        midrange_variables->last_was_detected[1] = midrange_variables->last_was_detected[0];
        midrange_variables->last_was_detected[0] = false;
    }
}

int main(int argc, char *argv[]) {
    system("mkdir -p tmp");
    char output_filepath[14] = "tmp";
    strcat(strcat(output_filepath, kPathSeparator), "out.track");
    FILE *output_ptr = fopen(output_filepath, "w+");

    FILE *song_file_ptr = NULL;

    if (argc > 1) {
        int opt;
        while ((opt = getopt(argc, argv, "i:o:")) != -1) {
            switch (opt) {
                case 'i':
                    {
                        int buffer_size = 55+251; // 55 chars for command + 251 chars for filepath

                        char command_buffer[buffer_size]; 
                        int make_command = snprintf(command_buffer, buffer_size, "ffmpeg -i \"%s\" -loglevel error -ac 1 -f f32le -ar 44100 -", optarg);
                        if (make_command < 0 || make_command >= buffer_size) {
                            printf("Error: Can't decode filename. Ensure the file path is at most 250 characters long.\n");
                            return 1;
                        }
                        song_file_ptr = popen(command_buffer, "r");
                    }
                    break;
                case 'o':
                    output_ptr = fopen(optarg, "w+");
                    if (output_ptr == NULL) {
                        printf("Error: output file can't be opened. Defaulting to tmp/out.track\n");
                        output_ptr = fopen(output_filepath, "w+");
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
    int c = fgetc(song_file_ptr);
    if (c == EOF) {
        printf("Error: couldn't open the input file.\n");
        return 1;
    }
    else
        ungetc(c, song_file_ptr);
    
    float sample;

    kiss_fft_scalar in[FRAME_SIZE];
    kiss_fft_cpx out[FRAME_SIZE];
    kiss_fftr_cfg fwd = kiss_fftr_alloc(FRAME_SIZE, 0, NULL, NULL);

    BeatNode_t *bass_start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    bass_start->next = NULL;

    BeatList_t *bass_beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    bass_beats->head = bass_start;
    bass_beats->tail = bass_start;

    BassVariables_t *bass_variables = (BassVariables_t*) malloc(sizeof(BassVariables_t));
    memset(bass_variables->last_averages, 1, sizeof(bass_variables->last_averages));
    memset(bass_variables->last_maximums, 1, sizeof(bass_variables->last_maximums));
    memset(bass_variables->last_increases, 1, sizeof(bass_variables->last_increases));
    memset(bass_variables->last_was_detected, false, sizeof(bass_variables->last_was_detected));
    memset(bass_variables->last_frequencies, 10, sizeof(bass_variables->last_frequencies));
    memset(bass_variables->last_total_increases, 1, sizeof(bass_variables->last_total_increases));
    bass_variables->last_maximum_index = 0;

    BeatNode_t *midrange_start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    midrange_start->next = NULL;

    BeatList_t *midrange_beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    midrange_beats->head = midrange_start;
    midrange_beats->tail = midrange_start;

    MidrangeVariables_t *midrange_variables = (MidrangeVariables_t*) malloc(sizeof(MidrangeVariables_t));
    memset(midrange_variables->last_increases, 1, sizeof(midrange_variables->last_increases));
    memset(midrange_variables->last_was_detected, false, sizeof(midrange_variables->last_was_detected));
    memset(midrange_variables->last_frequencies, 1, sizeof(midrange_variables->last_frequencies));

    int frame = 0;
    double magnitudes[FRAME_SIZE / 2];

    double bass_running_average = 30;
    double midrange_running_average = 1;
    while (1) {
        for (int i = 0; i < FRAME_SIZE; i++) {
            // hanning window weight
            float window_weight = powf(sinf(PI * i / FRAME_SIZE), 2);
            fread((void*)(&sample), sizeof(sample), 1, song_file_ptr);
            in[i] = (kiss_fft_scalar)(sample * window_weight);
        }

        if (feof(song_file_ptr)) 
            break;

        kiss_fftr(fwd, &in[0], &out[0]);

        double bass_frame_average = 1;
        double midrange_frame_average = 1;

        for (int i = 1; i <= FRAME_SIZE / 2; i++) { // full spectrum
        // for (int i = 1; i <= 9; i++) { // only frequencies needed for now
            magnitudes[i] = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
            if (i <= 9)
                bass_frame_average += magnitudes[i];
            else if (i >= 10 && i <= 89)
                midrange_frame_average += magnitudes[i];

        }
        bass_frame_average /= 9;
        midrange_frame_average /= 80;

        float bass_weight = frame < 80 ? frame / 4 + 2 : 50;
        float midrange_weight = bass_weight;
        if (bass_frame_average < bass_running_average)
            bass_weight *= 2;
        bass_running_average += ((bass_frame_average > 10 ? bass_frame_average : 10) - bass_running_average) / bass_weight;

        if (midrange_frame_average < midrange_running_average)
            midrange_weight *= 2;

        midrange_running_average += (midrange_frame_average - midrange_running_average) / midrange_weight;

        float time = (frame + 0.5) * FRAME_SIZE / 44100.;

        detect_bass(&magnitudes[0], bass_beats, time, bass_frame_average, bass_running_average, bass_variables);
        detect_midrange(&magnitudes[0], midrange_beats, time, midrange_frame_average, midrange_running_average, midrange_variables);

        frame++;
    }

    // for (BeatNode_t *current_node = bass_beats->head->next; current_node != NULL; current_node = current_node->next)
    //     fprintf(output_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);
    
    // for (BeatNode_t *current_node = midrange_beats->head->next; current_node != NULL; current_node = current_node->next)
    //     fprintf(output_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);

    for (BeatNode_t *current_node = bass_beats->head->next; current_node != NULL; current_node = current_node->next) {
        fwrite(&current_node->layer, sizeof(char), 1, output_ptr);
        fwrite(&current_node->time, sizeof(float), 1, output_ptr);
    }

    for (BeatNode_t *current_node = midrange_beats->head->next; current_node != NULL; current_node = current_node->next) {
        fwrite(&current_node->layer, sizeof(char), 1, output_ptr);
        fwrite(&current_node->time, sizeof(float), 1, output_ptr);
    }

    // Free memory

    for (BeatNode_t *current_node = bass_beats->head; current_node != NULL; current_node = current_node->next)
        free(current_node);
    for (BeatNode_t *current_node = midrange_beats->head; current_node != NULL; current_node = current_node->next)
        free(current_node);

    free(bass_beats);
    free(midrange_beats);
    free(bass_variables);
    free(midrange_variables);
    kiss_fftr_free(fwd);

    return 0;
}