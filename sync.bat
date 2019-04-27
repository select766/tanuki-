IF NOT DEFINED VSINSTALLDIR CALL "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" || GOTO ON_ERROR

devenv YaneuraOu.sln /Rebuild Release /Project tanuki-proxy || GOTO ON_ERROR

scp tanuki-proxy/bin/release/tanuki-proxy.exe ec2-user@%1:~/ || GOTO ON_ERROR
scp tanuki-proxy/bin/release/proxy-setting.xml ec2-user@%1:~/ || GOTO ON_ERROR

ssh ec2-user@%1 ./sync.sh || GOTO ON_ERROR

ECHO Succeeded...
EXIT /B 0

:ON_ERROR
ECHO Failed...
EXIT /B 1
