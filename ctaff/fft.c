#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "lib/kissfft/kiss_fftr.h"

typedef struct BeatNode {
    struct BeatNode *next;
    // struct BeatNode *prev;
    float time;
    int average;
} BeatNode_t;

typedef struct BeatList {
    struct BeatNode *head, *tail;
} BeatList_t;

void add_beat(BeatList_t *beats, float time, int average) {
    BeatNode_t *new_node = (BeatNode_t*) malloc(sizeof(BeatNode_t));
    new_node->time = time;
    new_node->average = average;
    
    beats->tail->next = new_node;
    beats->tail = new_node;
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        system("mkdir -p tmp");

        int buffer_size = 53+201; // 53 chars for command + 201 chars for filepath * 2

        char command_buffer[buffer_size]; 
        int make_command = snprintf(command_buffer, buffer_size, "ffmpeg -i \"%1$s\" -y -ac 1 -f f32le -ar 44100 tmp/temp.raw", argv[1]);
        if (make_command < 0 || make_command >= buffer_size) {
            printf("Error: Can't decode filename. Remove non-ascii characters and ensure the file path is at most 200 characters long.");
            return 1;
        }
        system(command_buffer);
    }

    const float PI = 3.1415927;

    FILE *file_ptr;
    
    file_ptr = fopen("tmp/temp.raw", "rb");

    if (file_ptr == NULL) {
        printf("Error: file can't be opened.");
        return 1;
    }

    FILE *label_ptr;
    label_ptr = fopen("_labels.txt", "w+");

    float sample;
    const int nfft = 2048;
    kiss_fft_scalar in[nfft];
    kiss_fft_cpx out[nfft];
    kiss_fftr_cfg fwd = kiss_fftr_alloc(nfft, 0, NULL, NULL);
    double last_average = 1;
    double last_maximum = 1;
    double last_last_average = 1;
    double last_last_maximum = 1;
    bool last_was_detected = false;
    bool last_last_was_detected = false;

    BeatNode_t *start = (BeatNode_t*) malloc(sizeof(BeatNode_t));

    start->next = NULL;

    BeatList_t *beats = (BeatList_t*) malloc(sizeof(BeatList_t));
    beats->head = start;
    beats->tail = start;

    int frame = 0;

    while (1) {
        for (int i = 0; i < nfft; i++) {
            // hanning window weight
            float window_weight = powf(sinf(PI * i / (float)nfft), 2);
            fread((void*)(&sample), sizeof(sample), 1, file_ptr);
            // printf("%f\n", sample);
            in[i] = (kiss_fft_scalar)(sample * window_weight);
        }
        if (feof(file_ptr)) {
            // printf("reached file end\n");
            break;
        }
        // if (frame == 500) break;

        kiss_fftr(fwd, &in[0], &out[0]);

        // printf("******************* %f seconds *******************\n", (frame + .5) * nfft / 44100.);
        double average = sqrt(out[0].r * out[0].r + out[0].i * out[0].i) * 5;
        double maximum = average;
        for (int i = 1; i < 11; i++) {
            double magnitude = sqrt(out[i].r * out[i].r + out[i].i * out[i].i);
            // printf("%f | %f\n", i*44100/(float)nfft, magnitude);
            average += magnitude;
            if (magnitude > maximum)
                maximum = magnitude;
        }
        average /= 11.;
        // printf("MAXIMUM: %f\nAVERAGE: %f\n", maximum, average);
        double average_increase_ratio = average / last_average;
        double maximum_increase_ratio = maximum / last_maximum;
        double last_average_increase_ratio = average / last_last_average;
        double last_maximum_increase_ratio = maximum / last_last_maximum;
        float time = (frame + .5) * nfft / 44100.;
        if (!last_was_detected && !last_last_was_detected) {
            /*
            MAYBE put all detected below with average below 80 in separate track for 2nd weaker bass track
            */
            // if (average_increase_ratio > 2 && last_average_increase_ratio > 2 && average > 30 && maximum > 100 && maximum_increase_ratio > 2 && last_average_increase_ratio > 2) {
            //     printf("\nDETECTED WITH LEVEL\t%f at %f\n\n", average, time);
            //     add_beat(beats, time, (int)average);
            //     last_last_was_detected = last_was_detected;
            //     last_was_detected = true;
            // }
            if (average_increase_ratio > 1.8 && last_average_increase_ratio > 1.7 && average > 50 && maximum > 150 && maximum_increase_ratio > 1.3 && last_maximum_increase_ratio > 1.3) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 1);
                last_was_detected = true;
                // printf("%d\n", 1);
            }
            else if (average_increase_ratio > 1.5 && last_average_increase_ratio > 1.4 && average > 65 && maximum > 200 && maximum_increase_ratio > 1.2 && last_maximum_increase_ratio > 1.2) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 2);
                last_was_detected = true;
                // printf("%d\n", 2);
            }
            else if (average_increase_ratio > 1.35 && last_average_increase_ratio > 1.25 && average > 80 && maximum > 250 && maximum_increase_ratio > 1.2 && last_maximum_increase_ratio > 1.2) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 3);
                last_was_detected = true;
                // printf("%d\n", 3);
            }
            else if (maximum > 300 && average_increase_ratio > 1.2 && last_average_increase_ratio > 1.1) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 4);
                last_was_detected = true;
                // printf("%d\n", 4);
            }
            else if (maximum > 250 && maximum_increase_ratio > 1.4 && last_maximum_increase_ratio > 1.2) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 5);
                last_was_detected = true;
                // printf("%d\n", 5);
            }
            else if (maximum > 300 && maximum_increase_ratio > 1.3 && last_maximum_increase_ratio > 1.2) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 6);
                last_was_detected = true;
                // printf("%d\n", 6);
            }
            else if (maximum > 350 && maximum_increase_ratio > 1.2 && last_maximum_increase_ratio > 1.1) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 7);
                last_was_detected = true;
                // printf("%d\n", 7);
            }
            else if (maximum > 400 && maximum_increase_ratio > 1.1 && last_maximum_increase_ratio > 1.1) {
                // printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
                add_beat(beats, time, 8);
                last_was_detected = true;
                // printf("%d\n", 8);
            }
            // else if (maximum > 500 && maximum_increase_ratio > 1.1 && last_maximum_increase_ratio > 1.1) {
            //     printf("\nDETECTED WITH LEVEL\t%f at %f\n", average, time);
                // add_beat(beats, time, (int)average);
            //     last_last_was_detected = last_was_detected;
            //     last_was_detected = true;
            // }
        }
        else if (last_was_detected && maximum_increase_ratio > 1.1 && average_increase_ratio > 1.1) {
            // delete last detected and override with this one
            // printf("\nDETECTED WITH LEVEL\t%f at %f (and overriding last detected)\n", average, time);
            // fprintf(label_ptr, "%f\t%f\t%d\n", (j + .5) * nfft / 44100., time);
            beats->tail->time = time;
            // beats->tail->average = (int)average;
            last_was_detected = true;
            // last_last_was_detected = false;
        }
        else {
            last_last_was_detected = last_was_detected;
            last_was_detected = false;
        }
        last_last_average = last_average;
        last_last_maximum = last_maximum;
        last_average = average;
        last_maximum = maximum;
        frame++;
    }
    for (BeatNode_t *current_node = beats->head->next; current_node != NULL; current_node = current_node->next) {
        // printf("%f %d\n", current_node->time, current_node->average);
        fprintf(label_ptr, "%f\t%f\t%d\n", current_node->time, current_node->time, current_node->average);
    }
    for (BeatNode_t *current_node = beats->head; current_node != NULL; current_node = current_node->next) {
        free(current_node);
    }
    free(beats);
    kiss_fftr_free(fwd);
    
    system("rm tmp/temp.raw");

    return 0;
}