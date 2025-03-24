#include<iostream>
#include<math.h>
#include<complex>
#include<SDL.h>
#include<fftw3.h>
#include<string>
#include<windows.h>



using namespace std;

#define RATE 44100
#define CHUNK 128//BUFFER SIZE
#define CHANNELS 1 //mono audio 
#define FORMAT AUDIO_S16SYS //SIGNED INTEGERS IN 16 BIT IN SYSTEM FORMAT
#define MAX_SAMPLE_VAL 32767 //2^16 max val a 16 bit integer can take used to represent audio
#define DATALEN 65536 //length of the signal on which fft will be done



typedef short sample;  //short is short int (a 16 bit signed integer)
typedef complex<double> cmplx;

sample currentaudio[DATALEN]; //ARRAY TO HOLD CURRENT AUDIO 
int writepos = 0; //incrementing this each time will avoid a partially filled array while wrighting data in array


struct Note {
    const char* name;
    float frequency;
};

Note allNotes[] = {
    {"C2", 65.41}, {"C#2", 69.30}, {"D2", 73.42}, {"D#2", 77.78}, {"E2", 82.41}, {"F2", 87.31},
    {"F#2", 92.50}, {"G2", 98.00}, {"G#2", 103.83}, {"A2", 110.00}, {"A#2", 116.54}, {"B2", 123.47},
    {"C3", 130.81}, {"C#3", 138.59}, {"D3", 146.83}, {"D#3", 155.56}, {"E3", 164.81}, {"F3", 174.61},
    {"F#3", 185.00}, {"G3", 196.00}, {"G#3", 207.65}, {"A3", 220.00}, {"A#3", 233.08}, {"B3", 246.94},
    {"C4", 261.63}, {"C#4", 277.18}, {"D4", 293.66}, {"D#4", 311.13}, {"E4", 329.63}, {"F4", 349.23},
    {"F#4", 369.99}, {"G4", 392.00}, {"G#4", 415.30}, {"A4", 440.00}, {"A#4", 466.16}, {"B4", 493.88},
    {"C5", 523.25}, {"C#5", 554.37}, {"D5", 587.33}, {"D#5", 622.25}, {"E5", 659.25}
    // Add more notes if needed
};

int allNotesCount = sizeof(allNotes) / sizeof(allNotes[0]);

void RecCallback(void* userdata, Uint8* stream, int streamlength){

    sample* newdata = (sample*)stream;
    int num_samples = streamlength/sizeof(sample);

    for(int i=0; i<num_samples; i++){
        currentaudio[writepos] = newdata[i];
        writepos = (writepos + 1)%DATALEN;
    }
    
}

void FindFreqContent(sample* output, sample* input, int n) {
    fftw_complex* fftin = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);
    fftw_complex* fftout = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * n);

    for (int i = 0; i < n; i++) {
        fftin[i][0] = input[i]; // Real part
        fftin[i][1] = 0.0;      // Imaginary part
    }

    fftw_plan plan = fftw_plan_dft_1d(n, fftin, fftout, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);

    double noiseThreshold = 10000.0; // Threshold to filter noise


    for (int i = 0; i < n; i++) {
        double magnitude = sqrt(fftout[i][0] * fftout[i][0] + fftout[i][1] * fftout[i][1]);
        if (magnitude < noiseThreshold) {
            magnitude = 0;
        }
        
        double scaled_value = magnitude / 200; // Scaling factor

        // Clamp the value to MAX_SAMPLE_VAL
        output[i] = (sample)(scaled_value > MAX_SAMPLE_VAL ? MAX_SAMPLE_VAL : scaled_value);

    }

    fftw_destroy_plan(plan);
    fftw_free(fftin);
    fftw_free(fftout);
}

float findDominantFrequency(sample* spectrum, int length, int sampleRate) {
    int maxIndex = 0;
    sample maxValue = 0;

    // Only look at the first half of the spectrum (up to Nyquist frequency)
    for (int i = 1; i < length / 2; i++) {
        if (spectrum[i] > maxValue) {
            maxValue = spectrum[i];
            maxIndex = i;
        }
    }

    // Convert bin index to frequency
    float frequency = (float)maxIndex * sampleRate / length;
    return frequency;
}

Note findNearestNote(float frequency, Note* tuning, int tuningSize) {
    Note nearest = tuning[0];
    float minCents = 1200; // Maximum possible distance in cents (an octave)

    for (int i = 0; i < tuningSize; i++) {
        // Calculate distance in cents (100 cents = 1 semitone)
        float cents = 1200 * log2(frequency / tuning[i].frequency);
        cents = fabs(cents);

        if (cents < minCents) {
            minCents = cents;
            nearest = tuning[i];
        }
    }

    return nearest;
}

string frequencyToNoteName(double freq) {
    if (freq <= 0) return "Invalid"; // Prevent log2(0) error

    string noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    int noteIndex = static_cast<int>(round(12 * log2(freq / 440.0))) + 9; // +9 shifts A4 to C4
    noteIndex = noteIndex % 12;


    if (noteIndex < 0) noteIndex += 12;  // Ensure it’s always positive

    return noteNames[noteIndex]; // Return valid note name
}

void checkTuningStatus(float detectedFreq) {
    Note nearestNote = findNearestNote(detectedFreq, allNotes, allNotesCount);
    float noteFreq = nearestNote.frequency;

    float tolerance = noteFreq * 0.01;  // ±1% tolerance
    float lowerBound = noteFreq - tolerance;
    float upperBound = noteFreq + tolerance; // Difference in Hz

    cout << "Detected Frequency: " << detectedFreq << " Hz" << endl;

    if (detectedFreq >= lowerBound && detectedFreq <= lowerBound) {
        cout << "Tuning Status: IN TUNE " << endl;
    }
    else if (detectedFreq < lowerBound) {
        cout << "Tuning Status: TOO FLAT  (Tune Up)" << endl;
    }
    else if (detectedFreq > lowerBound) {
        cout << "Tuning Status: TOO SHARP (Tune Down)" << endl;
    }
}


int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec RecAudioSpec;
    RecAudioSpec.freq = RATE;
    RecAudioSpec.format = FORMAT;
    RecAudioSpec.channels = CHANNELS;
    RecAudioSpec.samples = CHUNK;
    RecAudioSpec.callback = RecCallback;
   
    SDL_AudioDeviceID RecDevice = SDL_OpenAudioDevice(NULL, 1/*1-> device open for recording, 0-> close/default*/, &RecAudioSpec, NULL, 0);

    SDL_PauseAudioDevice(RecDevice, 0);

    char pitchnames[] = "A       A#      B       C       C#      D       D#      E       F       F#      G       G#      \n";
    int bargraph[96];
    float OneFortyEightthOfAnOctave = pow(2, 1.0 / 96.0);
    float PitchA1 = 55;
    sample spectrum[DATALEN];

    for (int i = 0; i < 1000; i++) {
        FindFreqContent(spectrum, currentaudio, DATALEN);

        for (int j = 0; j < 96; j++) {
            bargraph[j] = 0;
            float frequency = PitchA1 * pow(OneFortyEightthOfAnOctave, (float)j);
            float next_frequency = frequency * OneFortyEightthOfAnOctave;
            float spectral_index = frequency * (float)DATALEN / (float)RATE;
            float next_spectral_index = next_frequency * (float)DATALEN / (float)RATE;

            for (int k = 0; k < 8; k++) {
                for (int m = round(spectral_index); m < round(next_spectral_index); m++) {
                    if(m<DATALEN)
                        bargraph[j] += spectrum[m] / (8 * (next_spectral_index - spectral_index));
                }

                spectral_index *= 2;
                next_spectral_index *= 2;
            }
        }


        float dominantFreq = findDominantFrequency(spectrum, DATALEN, RATE);
        string noteName = frequencyToNoteName(dominantFreq);
       

        
        system("cls");

        cout << "Detected Frequency: " << dominantFreq << " Hz" << endl;
        cout << endl;
        cout << "Detected Note: " << noteName <<endl;
        cout << endl;
        checkTuningStatus(dominantFreq);  // Call the function to check tuning status
        
        cout << endl;
        cout << pitchnames;
        for (int i = 20; i > 0; i--) {
            for (int j = 0; j < 96; j++) {
                if (bargraph[j] > i * 200)
                    cout << '|';
                else
                    cout << ' ';
            }
            cout << '\n';
        }
    }



    SDL_CloseAudioDevice(RecDevice);
    return 0;
}
