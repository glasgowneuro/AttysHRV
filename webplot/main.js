function plot_data(r) {
    var miny = 0;
    var maxy = 200;

    var winx = $(window).width() * 0.8;
    var winy = $(window).height() / 4;

    var ecg_hr = new Dygraph(
        document.getElementById("ecg_hr"),
	r, {
	    ylabel: 'HR / bpm',
	    animatedZooms: true,
            drawPoints: true,
	    width: winx,
	    height: winy,
	    valueRange: [ 0, 200 ],
	    axes : {
                x : {
                    valueFormatter: Dygraph.dateString_,
                    ticker: Dygraph.dateTicker
                },
		y : {
		    axisLabelWidth: 50
		}
            }
	}
    );

    var reset = function() {
        var rng = ecg_hr.xAxisExtremes() 
        ecg_hr.updateOptions({dateWindow: rng});
    };

    var pan = function(dir) {
        var w = ecg_hr.xAxisRange();
        var scale = w[1] - w[0];
        var amount = scale * 0.25 * dir;
        var rng = [ w[0] + amount, w[1] + amount ];
        ecg_hr.updateOptions({dateWindow: rng});
    };

    document.getElementById('full').onclick = function() { reset(); };
    document.getElementById('left').onclick = function() { pan(-1); };
    document.getElementById('right').onclick = function() { pan(+1); };
}

function read_file_contents(fileobj) {
    if (fileobj) {
	var reader = new FileReader();
	reader.readAsText(fileobj, "UTF-8");
	reader.onload = function (evt) {
            document.getElementById("filename").innerHTML = fileobj.name;
	    plot_data(evt.target.result);
	}
	reader.onerror = function (evt) {
	    document.getElementById("message").innerHTML = "error reading file";
	}
    }
}

function upload_file(e) {
    e.preventDefault();
    fileobj = e.dataTransfer.files[0];
    read_file_contents(fileobj)
}

function file_explorer() {
    document.getElementById('selectfile').click();
    document.getElementById('selectfile').onchange = function() {
        fileobj = document.getElementById('selectfile').files[0];
	read_file_contents(fileobj)
    };
}
