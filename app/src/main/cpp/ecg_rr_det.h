#ifndef ECG_RR_DET_H
#define ECG_RR_DET_H

#include "Iir.h"

// R peak detector using a DB3 wavelet. Runs only at 250Hz.
class ECG_rr_det {

public:
    double getAmplitude() {
        return amplitude;
    }

public:
    // this is a callback which is called whenever an R peak is detected
    // gives back the sample number from last reset, the heartrate in bpm (filtered and unfiltered),
    // the amplitude of the R peak in arbitrary units and the confidence of the
    // detection: 1 means just OK, greater than one means more confident
    class RRlistener {
    public:
        virtual void hasRpeak(long samplenumber,
                              float bpm,
                              double amplitude,
                              double confidence) = 0;
    };

    // constructor
    ECG_rr_det(RRlistener* rRlistener);

    void init(float fs) {
        bandPass.setup(2,fs,20,15);
        highPass.setup(2,fs,5);
    }

    // reset detector
    void reset();

    // detect r peaks
    // input: ECG samples at the specified sampling rate and in V
    void detect(float v);

private:
    RRlistener* rrListener = nullptr;

    long timestamp = 0;

    // previous timestamp
    long t2 = 0;

    // previously detected heartrate
    float prevBPM = 0;

    // timewindow not to detect an R peak
    int doNotDetect = 0;

    // counts towards zero every RR peak
    // if non zero the RR value is ignored
    int ignoreRRvalue = 2;

    // ignores samples to let the filter settle
    int ignoreECGdetector = 1000;

    // adaptive amplitude value of the detector output
    double amplitude = 0.0;

    Iir::Butterworth::BandPass<2> bandPass;
    Iir::Butterworth::HighPass<2> highPass;

    // sampling rate in Hz
    float samplingRateInHz = 250;

    // how fast the adaptive threshold follows changes in ECG
    // amplitude. Realisic values: 0.1 .. 1.0
    // 0.1 = slow recovery after an artefact but no wrong detections
    // 1 = fast recovery after an artefact but possibly wrong detections
    const float adaptive_threshold_decay_constant = 0.25F;

    // the threshold for the detection is 0.6 times smaller than the amplitude
    const float threshold_factor = 0.6F;

    // 10mV as the threshold the bandpass filtered ECG is an artefact
    const double artefact_threshold = 10;



};

#endif
