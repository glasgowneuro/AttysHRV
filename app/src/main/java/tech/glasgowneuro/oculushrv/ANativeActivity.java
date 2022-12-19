package tech.glasgowneuro.oculushrv;

import android.util.Log;

import tech.glasgowneuro.attyscomm.AttysComm;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "OculusECG";

  static private long instance = 0;

  static AttysComm attysComm;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("oculushrv");
  }

  static native void dataUpdate(long inst, float v);

  static AttysComm.DataListener dataListener = new AttysComm.DataListener() {
    @Override
    public void gotData(long l, float[] f) {
      double v = f[AttysComm.INDEX_Analogue_channel_1];
      dataUpdate(instance, (float)v);
    }
  };

  static native void initJava2CPP(float fs);

  static void startAttysComm(long inst) {
    Log.d(TAG, "Starting AttysComm");
    instance = inst;
    attysComm = new AttysComm();
    attysComm.registerDataListener(dataListener);
    attysComm.setAdc_samplingrate_index(AttysComm.ADC_RATE_250HZ);
    initJava2CPP(attysComm.getSamplingRateInHz());
    attysComm.start();
  }

  static void stopAttysComm() {
    Log.d(TAG,"Stopping AttysComm");
    attysComm.stop();
  }
}
