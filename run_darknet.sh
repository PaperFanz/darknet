#!/bin/sh

./darknet detect cfg/yolov3-tiny.cfg yolov3-tiny.weights data/dog.jpg -save_labels
