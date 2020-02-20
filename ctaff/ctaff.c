#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/kissfft/kiss_fftr.h"

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
    double last_averages_high[2]; // {n-1, n-2}
    double last_maximums_high[2];
    double last_averages_low[2];
    double last_maximums_low[2];

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

bool detect_beat(double average, double maximum, double average_increase_ratios[2], double maximum_increase_ratios[2]) {
    if (average_increase_ratios[0] > 1.8 && average_increase_ratios[1] > 1.7 && average > 50 && maximum > 150 && maximum_increase_ratios[0] > 1.3 && maximum_increase_ratios[1] > 1.3) {
        return true;
    }
    else if (average_increase_ratios[0] > 1.5 && average_increase_ratios[1] > 1.4 && average > 65 && maximum > 200 && maximum_increase_ratios[0] > 1.2 && maximum_increase_ratios[1] > 1.2) {
        return true;
    }
    else if (average_increase_ratios[0] > 1.35 && average_increase_ratios[1] > 1.25 && average > 80 && maximum > 250 && maximum_increase_ratios[0] > 1.2 && maximum_increase_ratios[1] > 1.2) {
        return true;
    }
    /* average */
    // else if (maximum > 200 && average_increase_ratios[0] > 1.5 && average_increase_ratios[1] > 1.4) {
    //     return true;
    // }
    else if (maximum > 250 && average > 100 && average_increase_ratios[0] > 1.4 && average_increase_ratios[1] > 1.3) {
        return true;
    }
    else if (maximum > 300 && average > 100 && average_increase_ratios[0] > 1.2 && average_increase_ratios[1] > 1.1) {
        return true;
    }
    else if (maximum > 350 && average > 100 && average_increase_ratios[0] > 1.1 && average_increase_ratios[1] > 1.1) {
        return true;
    }
    /* maximum */
    else if (maximum > 200 && maximum_increase_ratios[0] > 1.5 && maximum_increase_ratios[1] > 1.8) {
        return true;
    }
    else if (maximum > 250 && maximum_increase_ratios[0] > 1.4 && maximum_increase_ratios[1] > 1.6) {
        return true;
    }
    else if (maximum > 300 && maximum_increase_ratios[0] > 1.3 && maximum_increase_ratios[1] > 1.4) {
        return true;
    }
    else if (maximum > 350 && maximum_increase_ratios[0] > 1.2 && maximum_increase_ratios[1] > 1.3) {
        return true;
    }
    return false;

}

void detect_bass(double *out, BeatList_t *beats, float time, BassVariables_t *bass_variables) {
    bool detected = false;
    double average = 0, maximum = 0, maximum_average = 0;
    int maximum_index = 1;
    if (time >= 17 && time <= 20)
    printf("******************* %f seconds *******************\n", time);
    //     for (int i = 100; i < 1024; i+=20) {
    //         double magnitude = out[i];
    //         if (time > 84 && time < 85)
    //         printf("%f | %f\n", i*44100/2048., magnitude);
    //     }
    // 

    // do higher frequencies
    for (int i = 0; i < 11; i++) {
        double magnitude = out[i];
        if (time >= 17 && time <= 20)
        printf("%f | %f\n", i*44100/2048., magnitude);
        average += magnitude;
        if (magnitude > maximum) {
        	maximum = magnitude;
        	maximum_index = i;
        }
    }
    maximum_average = (out[maximum_index + (maximum_index == 0 || out[maximum_index + 1] > out[maximum_index - 1] ? 1 : -1)] + maximum) / 2;
    average /= 11.;
    // if (time > 84 && time < 85)
    // printf("MAXIMUM: %f\nAVERAGE: %f\n", maximum, average);
    double average_increase_ratios[2] = {average / bass_variables->last_averages_high[0], average / bass_variables->last_averages_high[1]},
           maximum_increase_ratios[2] = {maximum_average / bass_variables->last_maximums_high[0], maximum_average / bass_variables->last_maximums_high[1]};

    
    if (!bass_variables->last_was_detected[0] && !bass_variables->last_was_detected[1]) {
        if (detect_beat(average, maximum_average, average_increase_ratios, maximum_increase_ratios)) {
            add_beat(beats, time, 'A');
            detected = true;
        }
    }
    else if (beats->tail->layer == 'A' && average_increase_ratios[0] + maximum_increase_ratios[0] > 2.1 && average_increase_ratios[1] + maximum_increase_ratios[1] > 2.1) {
        beats->tail->time = (beats->tail->time + time) / 2.;
        beats->tail->layer = 'C';
        bass_variables->last_was_detected[0] = true;
    }

    bass_variables->last_averages_high[1] = bass_variables->last_averages_high[0];
    bass_variables->last_averages_high[0] = average;
    bass_variables->last_maximums_high[1] = bass_variables->last_maximums_high[0];
    bass_variables->last_maximums_high[0] = maximum;

    // do lower frequencies

    average = 0, maximum = 0, maximum_index = 1, maximum_average = 0;
    for (int i = 0; i < 6; i++) {
        double magnitude = out[i];
        // printf("%f | %f\n", i*44100/(float)nfft, magnitude);
        average += magnitude;
        if (magnitude > maximum) {
        	maximum = magnitude;
        	maximum_index = i;
        }
    }
    maximum_average = (out[maximum_index + (maximum_index == 0 || out[maximum_index + 1] > out[maximum_index - 1] ? 1 : -1)] + maximum) / 2;
    average /= 6.;
    average_increase_ratios[0] = average / bass_variables->last_averages_low[0];
    average_increase_ratios[1] = average / bass_variables->last_averages_low[1];
    maximum_increase_ratios[0] = maximum_average / bass_variables->last_maximums_low[0];
    maximum_increase_ratios[1] = maximum / bass_variables->last_maximums_low[1];

    if (detect_beat(average, maximum_average, average_increase_ratios, maximum_increase_ratios)) {
        if (!detected && !bass_variables->last_was_detected[0] && !bass_variables->last_was_detected[1]) {
            add_beat(beats, time, 'B');
            detected = true;
        }
        else if (bass_variables->last_was_detected[0] && average_increase_ratios[0] > 1.1 && maximum_increase_ratios[0] > 1.1) {
            beats->tail->time = (beats->tail->time + time) / 2.;
            beats->tail->layer = 'D';
            // detected = true;
        }
    }
    // if (!detected && !bass_variables->last_was_detected[0] && !bass_variables->last_was_detected[1]) {
    //     if (detect_beat(average, maximum, average_increase_ratios, maximum_increase_ratios)) {
    //         add_beat(beats, time, 'B');
    //         detected = true;
    //     }
    // }
    // else if (bass_variables->last_was_detected[0] && beats->tail->layer == 'D' && (average_increase_ratios[0] > 1.1 || maximum_increase_ratios[0] > 1.1)) {
    //     beats->tail->time = (beats->tail->time + time) / 2.;
    //     beats->tail->layer = 'D';
    //     detected = true;
    // }

    bass_variables->last_averages_low[1] = bass_variables->last_averages_low[0];
    bass_variables->last_averages_low[0] = average;
    bass_variables->last_maximums_low[1] = bass_variables->last_maximums_low[0];
    bass_variables->last_maximums_low[0] = maximum;

    // end
    if (detected) {
        bass_variables->last_was_detected[0] = true;
    }
    else {
        bass_variables->last_was_detected[1] = bass_variables->last_was_detected[0];
        bass_variables->last_was_detected[0] = false;
    }
}

int main(int argc, char *argv[]) {
    const float PI = 3.1415927;

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
    const int nfft = 2048;
    kiss_fft_scalar in[nfft];
    kiss_fft_cpx out[nfft];
    kiss_fftr_cfg fwd = kiss_fftr_alloc(nfft, 0, NULL, NULL);

    BeatNode_t *start = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    start->next = NULL;

    BeatList_t *beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    beats->head = start;
    beats->tail = start;

    BassVariables_t *bass_variables = (BassVariables_t*) malloc(sizeof(BassVariables_t));
    memset(bass_variables->last_averages_high, 1, sizeof(bass_variables->last_averages_high));
    memset(bass_variables->last_maximums_high, 1, sizeof(bass_variables->last_maximums_high));
    memset(bass_variables->last_averages_low, 1, sizeof(bass_variables->last_averages_low));
    memset(bass_variables->last_maximums_low, 1, sizeof(bass_variables->last_maximums_low));
    memset(bass_variables->last_was_detected, false, sizeof(bass_variables->last_was_detected));

    int frame = 0;
    double magnitudes[nfft / 2];

    while (1) {
        for (int i = 0; i < nfft; i++) {
            // hanning window weight
            float window_weight = powf(sinf(PI * i / (float)nfft), 2);
            fread((void*)(&sample), sizeof(sample), 1, file_ptr);
            in[i] = (kiss_fft_scalar)(sample * window_weight);
        }

        if (feof(file_ptr)) 
            break;

        kiss_fftr(fwd, &in[0], &out[0]);

        // for (int i = 0; i <= nfft / 2; i++) // full spectrum
        for (int i = 0; i < 20; i++) // only frequencies needed for now
        	magnitudes[i] = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);

        float time = (frame + 0.5) * nfft / 44100.;
        detect_bass(&magnitudes[0], beats, time, bass_variables);

        frame++;
    }
    for (BeatNode_t *current_node = beats->head->next; current_node != NULL; current_node = current_node->next) {
        fprintf(label_ptr, "%f\t%f\t%c\n", current_node->time - .05, current_node->time + .05, current_node->layer);
    }
    for (BeatNode_t *current_node = beats->head; current_node != NULL; current_node = current_node->next) {
        free(current_node);
    }
    free(beats);
    kiss_fftr_free(fwd);

    return 0;
}