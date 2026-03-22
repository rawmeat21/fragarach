#!/bin/bash

echo "Running benign samples..."
for file in /opt/fragarach/samples/benign/*; do
    echo "Running $file"
    sudo ./fragarach "$file" 0
done

echo "Running malware samples..."
for file in /opt/fragarach/samples/malware/*; do
    echo "Running $file"
    sudo ./fragarach "$file" 1
done

echo "Done. Graphs saved to /opt/fragarach/raw/"