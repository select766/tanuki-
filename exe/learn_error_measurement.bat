..\..\YaneuraOu2016Engine\YaneuraOu.exe l

PUSHD eval
CALL backup.bat
POPD

..\..\YaneuraOu2016Engine\YaneuraOu.exe e

YaneuraOu-local-game-server.exe < yaneuraou-local-game-server.txt

PUSHD eval
CALL restore.bat
POPD

PAUSE
