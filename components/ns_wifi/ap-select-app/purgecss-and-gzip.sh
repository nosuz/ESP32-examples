#!/bin/bash

node_modules/purgecss/bin/purgecss.js --css dist/assets/*.css --content dist/index.html dist/assets/*.js --output dist/assets/
for f in $(find dist ! -name '*.gz' -type f -size +2k); do
    gzip --keep -f $f;
done
