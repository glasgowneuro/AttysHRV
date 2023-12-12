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

startdatetime = datetime.datetime.fromtimestamp(float(data[0,0])/1000.)

for i in data:
    customdate = datetime.datetime.fromtimestamp(float(i[0])/1000.)
    ts.append(customdate)
    hrs.append(i[1])

t = "Heartrate"
plt.figure(t)
plt.title(t)
plt.plot(ts,hrs)
plt.xlabel("Date/Time (starts at {})".format(startdatetime))
plt.ylabel("HR/BPM")
plt.ylim([0,130])

t0 = float(data[0,0])
tmax = 35

xlist = np.linspace(-tmax, tmax, 500)
ylist = np.linspace(-tmax, tmax, 500)
X, Y = np.meshgrid(xlist, ylist)
Z = X*Y

r = np.round(np.sqrt((X*1000)**2 + (Y*1000)**2)/1000).astype(int)
Z = data[r,1]

fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

#drawing contour plot
cb = ax.contour(X, Y, Z, 100)

ax.set_title('3D Contour Plot')
ax.set_xlabel('t/sec')
ax.set_ylabel('t/sec')
ax.set_zlabel('Heartrate/BPM')

plt.show()
