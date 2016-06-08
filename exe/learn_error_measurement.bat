..\..\YaneuraOu2016Engine\YaneuraOu.exe < YaneuraOu-learn.txt

PUSHD eval
CALL backup.bat
POPD

..\..\YaneuraOu2016Engine\YaneuraOu.exe < YaneuraOu-error-measurement.txt

PUSHD eval
CALL restore.bat
POPD

YaneuraOu-local-game-server.exe < yaneuraou-local-game-server.txt

PAUSE
