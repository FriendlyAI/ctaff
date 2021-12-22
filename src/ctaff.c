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
    double last_maximum;
    double last_total_increases[2];
    double last_frequencies[80];

    bool last_was_detected[2];
} MidrangeVariables_t;

typedef struct HighFrequencyVariables {
    double last_average;
    double last_total_increases[2];
    double last_frequencies[75];

    bool last_was_detected[3];
} HighFrequencyVariables_t;

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

bool detect_bass_beat(double running_average, double average, double maximum_average, double increase, double average_increase_ratios[2], double maximum_increase_ratios[2], BassVariables_t *bass_variables) {
    double average_requirement = 0.3 * running_average + 45;
    double increase_requirement = 1.2 * running_average + 115;
    double maximum_requirement = 0.8 * running_average + 65;
    double last_maximum_increase_requirement = -0.0025 * running_average + 1.45;
    double last_total_increase_requirement = -0.0025 * running_average + 0.825;

    return average > 40 && average > average_requirement * (1 - pow(increase / 1000., 2)) && 
           increase > 100  && increase > increase_requirement && 
           maximum_average > maximum_requirement &&
           increase / bass_variables->last_maximums[0] > last_maximum_increase_requirement &&
           increase > bass_variables->last_total_increases[0] * last_total_increase_requirement;
}

bool detect_midrange_beat(double running_average, double maximum_average, double increase, double last_total_increases[2]) {
    double maximum_requirement = 20 * log(running_average + .4) + 15;
    double increase_requirement = 7 * running_average + 20;

    return maximum_average > 15 && maximum_average > maximum_requirement * (1 - pow(increase / 2000., 2)) && 
           increase > 40 && increase > increase_requirement && increase > (last_total_increases[0] + last_total_increases[1]) * .4;
}

bool detect_high_frequency_beat(double running_average, double average, double increase, double last_total_increases[2]) {
    double average_increase_requirement = -0.003 * increase + 1.9;

    return increase > 100 && average > average_increase_requirement * running_average && increase > (last_total_increases[0] + last_total_increases[1]) * .4;
}

bool detect_bass(double *out, BeatList_t *beats, float time, double average, double running_average, BassVariables_t *bass_variables) {
    bool detected = false;
    double increase = 1, maximum_average = 1;
    int maximum_index = 0;

    for (int i = 1; i <= 8; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - bass_variables->last_frequencies[i - 1];
        double last_increase = bass_variables->last_increases[i - 1];
        double max_increase = freq_increase > last_increase ? freq_increase : last_increase;
        if (max_increase > 30 + running_average / 2) { // can raise to reduce false positives
            increase += max_increase;
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

    else if (!bass_variables->last_was_detected[0] && detect_bass_beat(running_average, average, maximum_average, increase, average_increase_ratios, maximum_increase_ratios, bass_variables)) {
        if (!bass_variables->last_was_detected[1]) {
            if (bass_variables->last_maximum_index > maximum_index && bass_variables->last_maximums[0] > running_average / 2)
                time -= FRAME_TIME / 2;

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
    bass_variables->last_was_detected[0] = detected;

    return detected;
}

bool detect_midrange(double *out, BeatList_t *beats, float time, double average, double running_average, MidrangeVariables_t *midrange_variables) {
    bool detected = false;
    double increase = 1, maximum_average = 1;
    int maximum_index = 0;

    for (int i = 9; i <= 88; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - midrange_variables->last_frequencies[i - 9];
        if (freq_increase > 5 + running_average * 2) {
            increase += freq_increase;
        }
        double magnitude_average = (out[i + 1] + magnitude) / 2;
        if (magnitude_average > maximum_average && i != 88) {
            maximum_average = magnitude_average;
            maximum_index = i;
        }
        midrange_variables->last_frequencies[i - 9] = magnitude;
    }

    if (detect_midrange_beat(running_average, maximum_average, increase, midrange_variables->last_total_increases)) {
        if (!midrange_variables->last_was_detected[0] && !midrange_variables->last_was_detected[1]) {
            char layer = 'C';
            if (maximum_index > 30)
                layer = 'E';
            else if (maximum_index > 16)
                layer = 'D';
            if (time - beats->tail->time <= .2 && beats->tail->layer == layer) {
                if (layer == 'D')
                    layer = 'C' + (maximum_index > 24 ? 2 : 0);
                else
                    layer = 'D';
            }
            add_beat(beats, time, layer);
            detected = true;
        }
        else if (midrange_variables->last_was_detected[0] && !midrange_variables->last_was_detected[1] && (increase > midrange_variables->last_total_increases[0] || maximum_average > midrange_variables->last_maximum)) {
            beats->tail->time += FRAME_TIME / 2;
            detected = true;
        }
    }

    midrange_variables->last_maximum = maximum_average;
    midrange_variables->last_total_increases[1] = midrange_variables->last_total_increases[0];
    midrange_variables->last_total_increases[0] = increase;

    midrange_variables->last_was_detected[1] = midrange_variables->last_was_detected[0];
    midrange_variables->last_was_detected[0] = detected;

    return detected;
}

bool detect_high_frequency(double *out, BeatList_t *beats, float time, double average, double running_average, HighFrequencyVariables_t *high_frequency_variables) {
    bool detected = false;

    double increase = 1;

    for (int i = 130; i <= 204; i++) {
        double magnitude = out[i];
        double freq_increase = magnitude - high_frequency_variables->last_frequencies[i - 130];
        if (freq_increase > running_average * 2) {
            increase += freq_increase;
        }
        high_frequency_variables->last_frequencies[i - 130] = magnitude;
    }

    if (detect_high_frequency_beat(running_average, average, increase, high_frequency_variables->last_total_increases)) {
        if (!high_frequency_variables->last_was_detected[0] && !high_frequency_variables->last_was_detected[1] && !high_frequency_variables->last_was_detected[2]) {
            add_beat(beats, time, 'F');
            detected = true;
        }
        else if (high_frequency_variables->last_was_detected[0] && !high_frequency_variables->last_was_detected[1] && increase > 150 && (increase > high_frequency_variables->last_total_increases[0] || average > high_frequency_variables->last_average)) {
            beats->tail->time += FRAME_TIME / 2;
            detected = true;
        }
    }

    high_frequency_variables->last_average = average;
    high_frequency_variables->last_total_increases[1] = high_frequency_variables->last_total_increases[0];
    high_frequency_variables->last_total_increases[0] = increase;

    high_frequency_variables->last_was_detected[2] = high_frequency_variables->last_was_detected[1];
    high_frequency_variables->last_was_detected[1] = high_frequency_variables->last_was_detected[0];
    high_frequency_variables->last_was_detected[0] = detected;

    return detected;
}

int main(int argc, char *argv[]) {
    FILE *output_ptr = NULL;
    FILE *song_file_ptr = NULL;

    if (argc > 2) {
        int opt;
        while ((opt = getopt(argc, argv, "i:o:")) != -1) {
            switch (opt) {
                case 'i':
                    {
                        int buffer_size = 54+351; // 54 chars for command + 351 chars for filepath

                        char command_buffer[buffer_size];
                        int make_command = snprintf(command_buffer, buffer_size, "ffmpeg -i \"%s\" -loglevel error -ac 1 -f f32le -ar 44100 -", optarg);
                        if (make_command < 0 || make_command >= buffer_size) {
                            printf("Error: Can't decode filename. Ensure the file path is at most 350 characters long.\n");
                            return 1;
                        }
                        song_file_ptr = popen(command_buffer, "r");
                    }
                    break;
                case 'o':
                    output_ptr = fopen(optarg, "w+");
                    if (output_ptr == NULL)
                        printf("Error: can't open output file.\n");
                    break;
            }
        }
    }
    else {
        printf("Error: no input file specified.\n");
        return 1;
    }
    
    if (output_ptr == NULL) {
        printf("Defaulting output to tmp/out.map\n");
        system("mkdir -p tmp");
        char output_filepath[14] = "tmp";
        strcat(strcat(output_filepath, kPathSeparator), "out.map");
        output_ptr = fopen(output_filepath, "w+");
    }

    // ensure decoding created output
    int c = fgetc(song_file_ptr);
    if (c == EOF) {
        printf("Error: couldn't open the input file.\n");
        return 1;
    }
    else
        ungetc(c, song_file_ptr);
    
    kiss_fft_scalar in[FRAME_SIZE];
    kiss_fft_cpx out[FRAME_SIZE];
    kiss_fftr_cfg fwd = kiss_fftr_alloc(FRAME_SIZE, 0, NULL, NULL);

    BeatNode_t *bass_start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    bass_start->next = NULL;
    bass_start->time = -1;

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
    midrange_start->time = -1;

    BeatList_t *midrange_beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    midrange_beats->head = midrange_start;
    midrange_beats->tail = midrange_start;

    MidrangeVariables_t *midrange_variables = (MidrangeVariables_t*) malloc(sizeof(MidrangeVariables_t));
    memset(midrange_variables->last_was_detected, false, sizeof(midrange_variables->last_was_detected));
    memset(midrange_variables->last_frequencies, 1, sizeof(midrange_variables->last_frequencies));
    memset(midrange_variables->last_total_increases, 1, sizeof(midrange_variables->last_total_increases));
    midrange_variables->last_maximum = 1;

    BeatNode_t *high_frequency_start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    high_frequency_start->next = NULL;
    high_frequency_start->time = -1;

    BeatList_t *high_frequency_beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    high_frequency_beats->head = high_frequency_start;
    high_frequency_beats->tail = high_frequency_start;

    HighFrequencyVariables_t *high_frequency_variables = (HighFrequencyVariables_t*) malloc(sizeof(HighFrequencyVariables_t));
    memset(high_frequency_variables->last_was_detected, false, sizeof(high_frequency_variables->last_was_detected));
    memset(high_frequency_variables->last_frequencies, 1, sizeof(high_frequency_variables->last_frequencies));
    memset(high_frequency_variables->last_total_increases, 1, sizeof(high_frequency_variables->last_total_increases));
    high_frequency_variables->last_average = 1;

    int frame = 0;
    double magnitudes[FRAME_SIZE / 2];

    double bass_running_average = 30;
    double midrange_running_average = 0;
    double high_frequency_running_average = 0;

    float sample;

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
        double high_frequency_frame_average = 1;

        for (int i = 1; i <= 204; i++) {
            magnitudes[i] = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
            if (i <= 8)
                bass_frame_average += magnitudes[i];
            else if (i >= 9 && i <= 88)
                midrange_frame_average += magnitudes[i];
            else if (i >= 130 && i <= 204)
                high_frequency_frame_average += magnitudes[i];
        }

        bass_frame_average /= 8;
        midrange_frame_average /= 80;
        high_frequency_frame_average /= 75;

        float bass_weight = frame < 80 ? frame / 4 + 2 : 50;
        float midrange_weight = bass_weight, high_frequency_weight = bass_weight;

        if (bass_frame_average < bass_running_average)
            bass_weight *= 2;
        bass_running_average += ((bass_frame_average > 10 ? bass_frame_average : 10) - bass_running_average) / bass_weight;

        if (midrange_frame_average < midrange_running_average)
            midrange_weight *= 2;
        midrange_running_average += (midrange_frame_average - midrange_running_average) / midrange_weight;

        if (high_frequency_frame_average < high_frequency_running_average)
            high_frequency_weight *= 2;
        high_frequency_running_average += (high_frequency_frame_average - high_frequency_running_average) / high_frequency_weight;

        float time = (frame + 0.5) * FRAME_SIZE / 44100.;

        if (detect_bass(&magnitudes[0], bass_beats, time, bass_frame_average, bass_running_average, bass_variables)) {
            if (fabsf(bass_beats->tail->time - midrange_beats->tail->time) < .05) {
                midrange_beats->tail->time = bass_beats->tail->time;
                midrange_variables->last_was_detected[0] = true;
            }
            if (fabsf(bass_beats->tail->time - high_frequency_beats->tail->time) < .05) {
                high_frequency_beats->tail->time = bass_beats->tail->time;
                high_frequency_variables->last_was_detected[0] = true;
            }
        }
        if (detect_midrange(&magnitudes[0], midrange_beats, time, midrange_frame_average, midrange_running_average, midrange_variables)) {
            if (fabsf(midrange_beats->tail->time - bass_beats->tail->time) < .05)
                midrange_beats->tail->time = bass_beats->tail->time;
            else if (fabsf(midrange_beats->tail->time - high_frequency_beats->tail->time) < .05) {
                float average_time = (high_frequency_beats->tail->time + midrange_beats->tail->time) / 2;
                midrange_beats->tail->time = average_time;
                high_frequency_beats->tail->time = average_time;
                high_frequency_variables->last_was_detected[0] = true;
            }
        }

        if (detect_high_frequency(&magnitudes[0], high_frequency_beats, time, high_frequency_frame_average, high_frequency_running_average, high_frequency_variables)) {
            if (fabsf(high_frequency_beats->tail->time - bass_beats->tail->time) < .05)
                high_frequency_beats->tail->time = bass_beats->tail->time;
            else if (fabsf(high_frequency_beats->tail->time - midrange_beats->tail->time) < .05) {
                float average_time = (high_frequency_beats->tail->time + midrange_beats->tail->time) / 2;
                midrange_beats->tail->time = average_time;
                high_frequency_beats->tail->time = average_time;
                midrange_variables->last_was_detected[0] = true;
            }
        }

        frame++;
    }

    // for (BeatNode_t *current_node = bass_beats->head->next; current_node != NULL; current_node = current_node->next)
    //     fprintf(output_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);
    
    // for (BeatNode_t *current_node = midrange_beats->head->next; current_node != NULL; current_node = current_node->next)
    //     fprintf(output_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);

    // for (BeatNode_t *current_node = high_frequency_beats->head->next; current_node != NULL; current_node = current_node->next)
    //     fprintf(output_ptr, "%f\t%f\t%c\n", current_node->time - FRAME_TIME, current_node->time + FRAME_TIME, current_node->layer);

    for (BeatNode_t *current_node = bass_beats->head->next; current_node != NULL; current_node = current_node->next) {
        fwrite(&current_node->layer, sizeof(char), 1, output_ptr);
        fwrite(&current_node->time, sizeof(float), 1, output_ptr);
    }

    for (BeatNode_t *current_node = midrange_beats->head->next; current_node != NULL; current_node = current_node->next) {
        fwrite(&current_node->layer, sizeof(char), 1, output_ptr);
        fwrite(&current_node->time, sizeof(float), 1, output_ptr);
    }

    for (BeatNode_t *current_node = high_frequency_beats->head->next; current_node != NULL; current_node = current_node->next) {
        fwrite(&current_node->layer, sizeof(char), 1, output_ptr);
        fwrite(&current_node->time, sizeof(float), 1, output_ptr);
    }

    // Free memory

    for (BeatNode_t *current_node = bass_beats->head; current_node != NULL; current_node = current_node->next)
        free(current_node);
    for (BeatNode_t *current_node = midrange_beats->head; current_node != NULL; current_node = current_node->next)
        free(current_node);
    for (BeatNode_t *current_node = high_frequency_beats->head; current_node != NULL; current_node = current_node->next)
        free(current_node);

    free(bass_beats);
    free(midrange_beats);
    free(high_frequency_beats);

    free(bass_variables);
    free(midrange_variables);
    free(high_frequency_variables);

    kiss_fftr_free(fwd);

    return 0;
}