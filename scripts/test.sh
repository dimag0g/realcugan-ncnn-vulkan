#!/bin/bash

# Generate test image
convert -size 50x50 canvas:white canvas:red canvas:lime canvas:blue canvas:black +append rgb.png
convert -size 50x50 canvas:black canvas:yellow canvas:magenta canvas:cyan canvas:white +append ymc.png
convert rgb.png ymc.png -append test.png
rm rgb.png ymc.png
convert test.png -scale 200% ref_x2.png
convert test.png -scale 400% ref_x4.png

# Test
test_iteration=1
while read -r scale tilesize noise model; do
    echo "
Running test $test_iteration: scale=$scale, noise=$noise, tilesize=$tilesize, model=$model"
    ./realcugan-ncnn-vulkan -s $scale -t $tilesize -n $noise -m $model -i test.png -o upscale$test_iteration.png > /dev/null || exit 1
    identify -format '%wx%h\n' upscale$test_iteration.png
    [ "$(identify -format '%wx%h\n' upscale$test_iteration.png)" = "$(identify -format '%wx%h\n' ref_x$scale.png)" ] || exit 2
    compare -metric AE -fuzz 5000 upscale$test_iteration.png ref_x$scale.png /dev/null 2>&1
    [ "$(compare -metric AE -fuzz 5000 upscale$test_iteration.png ref_x$scale.png /dev/null 2>&1 | cut -f 1 -d ' ')" -lt $(($scale*$scale*1250)) ] || exit 3
    test_iteration=$(($test_iteration+1))
# scale  tilesize noise  model
done << EOF
  2      0        -1     models-se
  4      0        -1     models-se
  2      32       -1     models-se
  2      200      -1     models-se
  2      0         0     models-se
  2      0         1     models-se
  2      0         2     models-se
  2      0         3     models-se
  2      0         0     models-nose
EOF
#todo: fix   2      32        0     models-pro
