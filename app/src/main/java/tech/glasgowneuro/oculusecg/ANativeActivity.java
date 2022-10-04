package tech.glasgowneuro.oculusecg;

import android.util.Log;

import tech.glasgowneuro.attyscomm.AttysComm;
import uk.me.berndporr.iirj.Butterworth;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "OculusECG";
  static final float powerlineHz = 50; //Hz

  static private long instance = 0;

  static AttysComm attysComm;

  final static private Butterworth dcFilter = new Butterworth();
  final static private Butterworth powerlineFilter = new Butterworth();

  static private ECG_rr_det ecg_rr_det_ch;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("oculusecg");
  }

  static native void dataUpdate(long inst, float v);

  static AttysComm.DataListener dataListener = new AttysComm.DataListener() {
    @Override
    public void gotData(long l, float[] f) {
      double v = f[AttysComm.INDEX_Analogue_channel_1];
      ecg_rr_det_ch.detect((float)v,false);
      v = dcFilter.filter(v);
      v = v * 1000;
      v = powerlineFilter.filter(v);
      dataUpdate(instance, (float)v);
    }
  };

  static native void hrUpdate(long inst, float v);

  static void startAttysComm(long inst) {
    Log.d(TAG,"Starting AttysComm");
    instance = inst;
    attysComm = new AttysComm();
    attysComm.registerDataListener(dataListener);
    dcFilter.highPass(2,attysComm.getSamplingRateInHz(),0.5);
    powerlineFilter.bandStop(2,attysComm.getSamplingRateInHz(),powerlineHz,2);
    ecg_rr_det_ch = new ECG_rr_det(attysComm.getSamplingRateInHz(), powerlineHz);
    ecg_rr_det_ch.setRrListener(new ECG_rr_det.RRlistener() {
      @Override
      public void haveRpeak(long samplenumber, float bpm, double amplitude, double confidence) {
        hrUpdate(instance,bpm);
        Log.d(TAG,"HR = "+bpm);
      }
    });
    attysComm.start();
  }

  static void stopAttysComm() {
    Log.d(TAG,"Stopping AttysComm");
    attysComm.stop();
  }
}
