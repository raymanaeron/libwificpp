#!/bin/bash
echo "Permanently deleting build folder..."
if [ -d "build" ]; then
    rm -rf build
    echo "Build folder deleted."
else
    echo "Build folder does not exist."
fi
echo "Done!"
