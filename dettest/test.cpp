#include "../app/src/main/cpp/ecg_rr_det.h"

#include <stdio.h>
#include "Iir.h"

struct MyCallback : ECG_rr_det::RRlistener {
	FILE* f;
	MyCallback(FILE* hrFile) {
		f = hrFile;
	}
	virtual void hasRpeak(long,
                              float hr,
                              double,
			      double) {
		printf("HR = %f\n",hr);
		fprintf(f,"%f\n",hr);
	}
};

int main (int argn,char** argv)
{
	if (argn < 2) {
		fprintf(stderr,"Filename please.\n");
		exit(1);
	}
	const float fs = 250;
	const float mains = 50;
	Iir::Butterworth::BandStop<2> iirnotch;
	iirnotch.setup(fs,mains,2);

	FILE* f = fopen("hr.dat","wt");
	if (!f) {
		fprintf(stderr,"Could not open hr.dat\n");
		exit(1);
	}

	MyCallback callback(f);

	ECG_rr_det rr_det(&callback);
	rr_det.init(fs);

	FILE *finput = fopen(argv[1],"rt");
	for(;;) 
	{
		float a1;
		if (fscanf(finput,"%f\n",&a1)<1) break;
		const float a = iirnotch.filter(a1);
		rr_det.detect(a);
	}
	fclose(finput);

	fclose(f);
}
