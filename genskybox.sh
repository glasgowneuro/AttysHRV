convert skybox.jpg -crop 1024x1024+0+1024 /tmp/left.ppm
convert skybox.jpg -crop 1024x1024+1024+1024 /tmp/front.ppm
convert skybox.jpg -crop 1024x1024+2048+1024 /tmp/right.ppm
convert skybox.jpg -crop 1024x1024+3072+1024 /tmp/back.ppm
convert skybox.jpg -crop 1024x1024+1024+0 /tmp/top.ppm
convert skybox.jpg -crop 1024x1024+1024+2048 /tmp/bottom.ppm
