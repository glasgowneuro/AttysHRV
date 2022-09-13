package tech.glasgowneuro.oculusecg;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import tech.glasgowneuro.attyscomm.AttysComm;
import tech.glasgowneuro.oculusecg.databinding.ActivityMainBinding;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "OculusECG";

  static private long instance = 0;

  static AttysComm attysComm;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("oculusecg");
  }

  static native void dataUpdate(long inst, float v);

  static AttysComm.DataListener dataListener = new AttysComm.DataListener() {
    @Override
    public void gotData(long l, float[] f) {
      dataUpdate(instance, f[AttysComm.INDEX_Analogue_channel_1]);
    }
  };

  static void startAttysComm(long inst) {
    instance = inst;
    attysComm = new AttysComm();
    attysComm.registerDataListener(dataListener);
    attysComm.start();
  }

  static void stopAttysComm() {
    attysComm.stop();
  }
}
