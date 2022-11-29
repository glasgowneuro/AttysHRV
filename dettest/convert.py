import numpy as np
from scipy import signal

a = np.loadtxt("ecg1.dat")
a = a[:,2]
print(len(a))
a = signal.decimate(a,4) / 500.0
print(len(a))
np.savetxt("sampleecg1.dat",a)
