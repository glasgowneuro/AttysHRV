#!/usr/bin/python3
import scipy.io.wavfile as wavfile
import matplotlib.pyplot as plt
import scipy
import numpy as np
import sys
import datetime
 
ts = []
hrs = []

data = np.loadtxt("attyshrv_heartrate.tsv")

for i in data:
    customdate = datetime.datetime.fromtimestamp(float(i[0])/1000.)
    ts.append(customdate)
    hrs.append(i[1])

t = "Heartrate"
plt.figure(t)
plt.title(t)
plt.plot(ts,hrs)
plt.xlabel("Date/time")
plt.ylabel("HR/BPM")
plt.ylim([0,200])

plt.show()
