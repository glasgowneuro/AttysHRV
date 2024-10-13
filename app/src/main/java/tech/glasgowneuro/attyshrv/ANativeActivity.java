package tech.glasgowneuro.attyshrv;

import android.content.Context;
import android.media.MediaScannerConnection;
import android.os.Bundle;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.util.Locale;

import tech.glasgowneuro.attyscomm.AttysComm;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "AttysHRV";
  static final String HR_FILE = "attyshrv_heartrate.tsv";

  static private long instance = 0;

  static AttysComm attysComm;

  static PrintWriter rawdatalog = null;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("attyshrv");
  }

  static native void setHRfilePath(String path);

  @Override
  protected void onCreate (Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    File fullpath = new File(getBaseContext().getExternalFilesDir(null), HR_FILE);
    String full_hr_file_path = fullpath.getAbsolutePath();
    Log.d(TAG,"Full path to local dir: "+full_hr_file_path);
    setHRfilePath(full_hr_file_path);
    // raw data
    try {
      File logFile = new File(getBaseContext().getExternalFilesDir(null), "raw.csv");
      rawdatalog =  new PrintWriter(new FileOutputStream(fullpath, true));
    }
    catch (IOException e) {
      Log.e(TAG, "Raw log file could not be opened: ", e);
    }
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    rawdatalog.close();
  }

  static native void dataUpdate(long inst, float v);

  static AttysComm.DataListener dataListener = new AttysComm.DataListener() {
    @Override
    public void gotData(long l, float[] f) {
      double v = f[AttysComm.INDEX_Analogue_channel_1];
      dataUpdate(instance, (float) v);
      String s = String.format(Locale.US, "%d,%f,%f\n",
              System.currentTimeMillis(), v, f[AttysComm.INDEX_Analogue_channel_2]);
      rawdatalog.write(s);
      rawdatalog.flush();
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
