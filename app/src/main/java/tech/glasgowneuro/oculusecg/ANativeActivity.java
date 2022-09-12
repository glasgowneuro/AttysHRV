package tech.glasgowneuro.oculusecg;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import tech.glasgowneuro.attyscomm.AttysComm;
import tech.glasgowneuro.oculusecg.databinding.ActivityMainBinding;

public class ANativeActivity extends android.app.NativeActivity {
  static final String TAG = "OculusECG";

  static AttysComm attysComm;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("oculusecg");
  }

  static native void dataUpdate(double data);

  static AttysComm.DataListener dataListener = new AttysComm.DataListener() {
    @Override
    public void gotData(long l, float[] f) {
      dataUpdate(f[AttysComm.INDEX_Analogue_channel_1]);
    }
  };

  static void startAttysComm() {
    attysComm = new AttysComm();
    attysComm.registerDataListener(dataListener);
    attysComm.start();
  }

  void stopAttysComm() {
    attysComm.stop();
  }
}
