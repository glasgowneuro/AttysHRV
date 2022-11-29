#include "ecg_rr_det.h"

#include <math.h>

ECG_rr_det::ECG_rr_det(RRlistener* _rRlistener) {
	rrListener = _rRlistener;
	reset();
}

void ECG_rr_det::reset() {
        amplitude = 0;
        t2 = 0;
        timestamp = 0;
        doNotDetect = (int) samplingRateInHz;
        ignoreECGdetector = (int) samplingRateInHz;
}

// detect r peaks
// input: ECG samples at the specified sampling rate and in V
void ECG_rr_det::detect(float v) {
	double h = 1000 * (double) v;
	h = highPass.filter(h);
	h = bandPass.filter(h);
	if (ignoreECGdetector > 0) {
		ignoreECGdetector--;
		return;
	}
	h = h * h;
	if (sqrt(h) > artefact_threshold) {
		// ignore signal for 1 sec
		ignoreECGdetector = ((int) samplingRateInHz);
		//Log.d(TAG,"artefact="+(Math.sqrt(h)));
		ignoreRRvalue = 2;
		return;
	}
	if (h > amplitude) {
		amplitude = h;
	}
	amplitude = amplitude - adaptive_threshold_decay_constant * amplitude / samplingRateInHz;
	
	if (doNotDetect > 0) {
		doNotDetect--;
	} else {
		double threshold = threshold_factor * amplitude;
		if (h > threshold) {
			float t = (float)(timestamp - t2) / samplingRateInHz;
			float bpm = 1 / t * 60;
			if ((bpm > 30) && (bpm < 250)) {
				if (ignoreRRvalue > 0) {
					ignoreRRvalue--;
				} else {
					if (bpm > 0) {
						if (((bpm * 1.5) < prevBPM) || ((bpm * 0.75) > prevBPM)) {
							ignoreRRvalue = 3;
						} else {
							rrListener->hasRpeak(timestamp,
												 bpm,
												 amplitude, h / threshold);
						}
						prevBPM = bpm;
					}
				}
			} else {
				ignoreRRvalue = 3;
			}
			t2 = timestamp;
			// advoid 1/5 sec
			doNotDetect = (int) samplingRateInHz / 5;
		}
	}
	timestamp++;
}
