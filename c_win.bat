@echo off
echo Permanently deleting build folder...
if exist build (
    rmdir /s /q build
    echo Build folder deleted.
) else (
    echo Build folder does not exist.
)
echo Done!
