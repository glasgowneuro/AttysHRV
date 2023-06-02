package tech.glasgowneuro.attyshrv;

import android.os.Bundle;
import android.util.Log;

import java.io.File;

import tech.glasgowneuro.attyscomm.AttysComm;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "AttysHRV";
  static final String HR_FILE = "attyshrv_heartrate.tsv";

  static private long instance = 0;

  static AttysComm attysComm;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("attyshrv");
  }

  static native void setHRfilePath(String path);

  @Override
  protected void onCreate (Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    File f = new File(getBaseContext().getExternalFilesDir(null), HR_FILE);
    String full_hr_file_path = f.getAbsolutePath();
    Log.d(TAG,"Full path to local dir: "+full_hr_file_path);
    setHRfilePath(full_hr_file_path);
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
    if (AttysComm.findAttysBtDevice() == null) {
      initJava2CPP(0);
      return;
    }
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
